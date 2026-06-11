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
#endif
#if defined(CONFIG_HAVE_MBEDTLS)
    #include <mbedtls/md.h>
    #include <mbedtls/private/pkcs5.h>  /* mbedtls_pkcs5_pbkdf2_hmac_ext() */
    #include <psa/crypto.h>             /* psa_generate_random(), psa_crypto_init() */
    /* Compatibility shim: callers use EVP_MAX_MD_SIZE for hash output buffers */
    #ifndef EVP_MAX_MD_SIZE
    #define EVP_MAX_MD_SIZE MBEDTLS_MD_MAX_SIZE
    #endif
#endif
#if !defined(CONFIG_HAVE_OPENSSL) && !defined(CONFIG_HAVE_MBEDTLS)
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
#include "c_prot_http_cl.h"
#include "c_task.h"
#include "c_tcp.h"
#include "c_authz.h"

#include "treedb_schema_authzs.c"
#include "../../libjwt/src/jwt-private.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
/*
 *  IdP user provisioning (register-idp-user).  The admin connection,
 *  config and machinery live here in C_AUTHZ so every yuno with auth
 *  inherits it; deploy-specific values are configured at runtime with
 *  set-kc-config (persistent attrs), never in code or committed config.
 */
#define KC_TOKEN_SKEW_S         30      /* refresh the admin token this many seconds before expiry */
#define KC_MAX_PENDING          32      /* reject register-idp-user beyond this queue depth */

/***************************************************************************
 *              Structures
 ***************************************************************************/
/*
 *  One queued register-idp-user request waiting for its Keycloak
 *  round-trips.  The requester is re-resolved by name at answer time so
 *  a client that disconnected mid-flight is never dereferenced.
 */
typedef struct _KC_PENDING {
    DL_ITEM_FIELDS
    char    email[256];
    char    first_name[128];
    char    last_name[128];
    char    role[128];          /* role id chosen by the caller ("" = none) */
    char    user_id[128];       /* KC user id, from the 201 Location header */
    char    req_service[80];    /* input gate service of the requester */
    char    req_channel[80];    /* requester channel name */
    json_t *kw_request;         /* original request kw (incref), for answer metadata */
    BOOL    authz_ok;           /* local treedb_authzs user created */
    BOOL    answered;           /* answer already sent */
} KC_PENDING;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int add_user_login(
    hgobj gobj,
    const char *username,
    json_t *jwt_payload,
    const char *peername
);

// PRIVATE json_t *identify_system_user(
//     hgobj gobj,
//     const char **username,
//     BOOL include_groups,
//     BOOL verbose
// );

PRIVATE json_t *get_user_roles(
    hgobj gobj,
    const char *dst_realm_id,
    const char *dst_service,
    const char *username,
    json_t *kw,         // not owned
    BOOL *superuser     // out, optional: set TRUE if user holds a service="*" role
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
PRIVATE json_t *cmd_set_max_sessions(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

/*  IdP (Keycloak) user provisioning. */
PRIVATE json_t *cmd_set_kc_config(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_kc_config(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_register_idp_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *kc_config_snapshot(hgobj gobj);
PRIVATE BOOL ensure_kc_client(hgobj gobj);
PRIVATE void process_next_kc(hgobj gobj);
PRIVATE void kc_answer(hgobj gobj, KC_PENDING *p, int result,
    json_t *jn_comment, json_t *jn_data, const char *warning);
PRIVATE json_t *kc_get_token(hgobj gobj, const char *lmethod, json_t *kw, hgobj src_task);
PRIVATE json_t *kc_save_token(hgobj gobj, const char *lmethod, json_t *kw, hgobj src_task);
PRIVATE json_t *kc_create_user(hgobj gobj, const char *lmethod, json_t *kw, hgobj src_task);
PRIVATE json_t *kc_create_user_result(hgobj gobj, const char *lmethod, json_t *kw, hgobj src_task);
PRIVATE json_t *kc_send_email(hgobj gobj, const char *lmethod, json_t *kw, hgobj src_task);
PRIVATE json_t *kc_send_email_result(hgobj gobj, const char *lmethod, json_t *kw, hgobj src_task);
PRIVATE int ac_end_task(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src);
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src);

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

PRIVATE sdata_desc_t pm_max_sessions[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,      0,          "Username, if empty then set gclass's attribute max_sessions_per_user"),
SDATAPM (DTP_INTEGER,   "max_sessions", 0,     "0",         "Max sessions"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_set_kc_config[] = {
/*-PM----type-----------name----------------------flag---default-description---------- */
SDATAPM (DTP_STRING,    "kc_base_url",            0,      0,      "Keycloak base URL, e.g. https://auth.example.com"),
SDATAPM (DTP_STRING,    "kc_realm",               0,      0,      "Keycloak realm"),
SDATAPM (DTP_STRING,    "kc_admin_client_id",     0,      0,      "Confidential admin client_id (client_credentials, manage-users)"),
SDATAPM (DTP_STRING,    "kc_admin_client_secret", 0,      0,      "Admin client secret"),
SDATAPM (DTP_STRING,    "kc_redirect_uri",        0,      0,      "redirect_uri for the set-password invite email"),
SDATAPM (DTP_STRING,    "kc_email_client_id",     0,      0,      "client_id the invite email links to (the SPA client)"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_register_idp_user[] = {
/*-PM----type-----------name------------flag------------default-description---------- */
SDATAPM (DTP_STRING,    "email",        SDF_REQUIRED,   0,      "Email (used as username)"),
SDATAPM (DTP_STRING,    "firstName",    0,              "",     "First name"),
SDATAPM (DTP_STRING,    "lastName",     0,              "",     "Last name"),
SDATAPM (DTP_STRING,    "role",         0,              "",     "Role id to assign in treedb_authzs"),
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
SDATACM2(DTP_SCHEMA,    "set-max-sessions", SDF_AUTHZ_X,0,      pm_max_sessions, cmd_set_max_sessions,"Set max session per user"),

SDATACM2(DTP_SCHEMA,    "set-kc-config",  SDF_AUTHZ_X,  0,  pm_set_kc_config,    cmd_set_kc_config,    "Configure & persist the Keycloak admin connection (register-idp-user). Only the params you pass are updated."),
SDATACM2(DTP_SCHEMA,    "view-kc-config", SDF_AUTHZ_X,  0,  0,                   cmd_view_kc_config,   "Show the persisted Keycloak admin config (secret masked)."),
SDATACM2(DTP_SCHEMA,    "register-idp-user", SDF_AUTHZ_X,0, pm_register_idp_user,cmd_register_idp_user,"Register a user in the IdP (Keycloak) + the local authz record; the user gets an email to set their password."),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
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

SDATA (DTP_STRING,  "authz_yuno_role",  SDF_RD|SDF_DEPRECATED, "", "If tranger_path is empty you can force the yuno_role where build the authz. If authz_yuno_role is empty get it from this yuno. DEPRECATED: use authz_service"),

SDATA (DTP_STRING,  "authz_tenant",     SDF_RD,     "",         "Used for multi-tenant service"),
SDATA (DTP_BOOLEAN, "master",           SDF_RD,     "0",        "the master is the only that can write, if tranger_path is empty is set to TRUE internally"),

SDATA (DTP_BOOLEAN, "allow_anonymous_in_localhost",SDF_RD,"0",  "Allow no user in local connections"),
SDATA (DTP_INTEGER, "max_sessions_per_user",SDF_PERSIST,    "0",        "Max sessions per user (0 no limit)"),
SDATA (DTP_JSON,    "jwks",                 SDF_WR|SDF_PERSIST, "[]",   "JWKS public keys, OLD jwt_public_keys, use the utility keycloak_pkey_to_jwks to create."),
SDATA (DTP_JSON,    "initial_load",         SDF_RD,         "{}",       "Initial data for treedb"),

SDATA (DTP_INTEGER, "hashIterations",   0,          "27500",    "Default To build a password"),
SDATA (DTP_STRING,  "algorithm",        0,          "sha256",   "Default To build a password"),

/*  IdP (Keycloak) admin connection for register-idp-user.  Neutral
 *  defaults on purpose: configure at runtime with set-kc-config, which
 *  persists them (SDF_PERSIST → auto-loaded on restart).  No identity/
 *  secret-bearing default in the code or in committed config.  The secret
 *  is masked by view-kc-config and could later be stored encrypted. */
SDATA (DTP_STRING,  "kc_base_url",           SDF_PERSIST, "",   "Keycloak base URL for the admin REST API (set via set-kc-config)"),
SDATA (DTP_STRING,  "kc_realm",              SDF_PERSIST, "",   "Keycloak realm where users are created"),
SDATA (DTP_STRING,  "kc_admin_client_id",    SDF_PERSIST, "",   "Confidential admin client_id (client_credentials, role manage-users)"),
SDATA (DTP_STRING,  "kc_admin_client_secret",SDF_PERSIST, "",   "Admin client secret (persisted; never in code or committed config)"),
SDATA (DTP_STRING,  "kc_redirect_uri",       SDF_PERSIST, "",   "redirect_uri for the set-password invite email"),
SDATA (DTP_STRING,  "kc_email_client_id",    SDF_PERSIST, "",   "client_id the invite email links to (the SPA client)"),
SDATA (DTP_JSON,    "kc_crypto",             SDF_RD,      "{\"ssl_use_system_ca\": true, \"ssl_verify_mode\": \"required\"}", "TLS crypto for KC outbound calls. Verifying-by-default against the system CA (public KC). For a private/self-signed KC CA, override with {\"ssl_trusted_certificate\":\"/path/ca.pem\"}. mbedTLS has no system store: set ssl_trusted_certificate there"),
SDATA (DTP_INTEGER, "kc_timeout_ms",         SDF_RD,      "30000","KC outbound round-trip watchdog (ms)"),

SDATA (DTP_POINTER, "user_data",        0,          0,          "user data"),
SDATA (DTP_POINTER, "user_data2",       0,          0,          "more user data"),
SDATA (DTP_POINTER, "subscriber",       0,          0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass authz levels
 *  Only the IdP-provisioning commands are enforced here (the legacy
 *  SDF_AUTHZ_X path in command_parser is disabled framework-wide); root
 *  and any role with permission "*" on this service pass.
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATAAUTHZ (DTP_SCHEMA, "register-idp-user", 0,      0,      0,  "Permission to register a user in the IdP + local authz"),
SDATAAUTHZ (DTP_SCHEMA, "configure-kc",      0,      0,      0,  "Permission to read/change the Keycloak admin config"),
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
    json_int_t max_sessions_per_user;

    hgobj gobj_tranger;
    hgobj gobj_treedb;
    json_t *tranger;
    BOOL master;

    json_t *jn_validations; // Set of keys with their checker
    jwk_set_t *jwks;        // Set of jwk

    /*  IdP (Keycloak) user provisioning — register-idp-user.  One shared
     *  outbound HTTP client (created lazily), requests serialized through
     *  dl_kc_pending and driven by a volatile multi-job C_TASK; the admin
     *  bearer token is cached until just before expiry. */
    hgobj       gobj_kc;                /* C_PROT_HTTP_CL to kc_base_url */
    char        kc_name[NAME_MAX];      /* base name for kc gobjs */
    char        kc_token[8192];         /* cached admin bearer token ("" = none) */
    time_t      kc_token_expiry;        /* epoch s; refresh when now >= expiry - skew */
    BOOL        kc_processing;          /* one KC round-trip task in flight */
    hgobj       gobj_kc_task;           /* current volatile C_TASK */
    dl_list_t   dl_kc_pending;          /* queued KC_PENDING requests */
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

    /*  IdP provisioning: the outbound HTTP client is created lazily by
     *  ensure_kc_client() on first register-idp-user, so it picks up the
     *  kc_base_url configured at runtime (set-kc-config, persistent). */
    dl_init(&priv->dl_kc_pending, gobj);
    snprintf(priv->kc_name, sizeof(priv->kc_name), "%s-kc", gobj_name(gobj));

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
#if defined(CONFIG_HAVE_MBEDTLS)
    /* Safe to call alongside OpenSSL — PSA and OpenSSL use independent state */
    psa_crypto_init(); /* PSA must be initialised before any crypto in v4.0 */
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
            authz_service = gobj_read_str_attr(gobj, "authz_yuno_role"); // DEPRECATED
            if(empty_string(authz_service)) {
                authz_service = gobj_yuno_role();
            }
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
            "path",         "%d", path,
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
            "msgset",       "%s", MSGSET_APP,
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*  Drain any never-answered register-idp-user requests. */
    KC_PENDING *p;
    while((p = dl_first(&priv->dl_kc_pending))) {
        dl_delete(&priv->dl_kc_pending, p, 0);
        JSON_DECREF(p->kw_request)
        GBMEM_FREE(p)
    }

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

    if(priv->gobj_kc_task && gobj_is_running(priv->gobj_kc_task)) {
        gobj_stop(priv->gobj_kc_task);
    }
    priv->gobj_kc_task = NULL;
    if(priv->gobj_kc && gobj_is_running(priv->gobj_kc)) {
        gobj_stop(priv->gobj_kc);
    }

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
            "msg",          "%s", "Peername is required",
            "username",     "%s", username,
            NULL
        );
        json_t *jn_resp = json_pack("{s:i, s:s, s:s}",
            "result", -1,
            "comment", "Peername is required",
            "username", username
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
            "username",     "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
        json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
            "result", -1,
            "comment", "Destination service not found",
            "username", username,
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
            "username",     "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
        json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
            "result", -1,
            "comment", "Ip denied",
            "username", username,
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
                     *  check_password() returned < 0 — wrong user/password
                     *  combination.  Log a warning so this lands in the
                     *  per-msg stats counters alongside the rest of the
                     *  failure paths in mt_authenticate (every other
                     *  result:-1 branch already logs one).
                     */
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_AUTH,
                        "msg",          "%s", "Invalid username or password",
                        "username",     "%s", username,
                        "service",      "%s", dst_service,
                        NULL
                    );
                    json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
                        "result", -1,
                        "comment", "Invalid username or password.",
                        "username", username,
                        "service", dst_service
                    );
                    KW_DECREF(kw)
                    return jn_resp;
                }

                comment = "User authenticated by password";
                break;
            }

            BOOL allow_anonymous_in_localhost = gobj_read_bool_attr(
                gobj, "allow_anonymous_in_localhost"
            );
            if(!allow_anonymous_in_localhost) {
                if(strcmp(username, "yuneta")!=0) {
                    /*
                     *  Only yuneta is allowed without jwt/passw
                     */
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_AUTH,
                        "msg",          "%s", "Without JWT/passw only yuneta is allowed",
                        "username",     "%s", username,
                        "service",      "%s", dst_service,
                        NULL
                    );
                    json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
                        "result", -1,
                        "comment", "Without JWT/passw only yuneta is allowed",
                        "username", username,
                        "service", dst_service
                    );
                    KW_DECREF(kw)
                    return jn_resp;
                }
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
                    "username",     "%s", username,
                    "service",      "%s", dst_service,
                    NULL
                );
                json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
                    "result", -1,
                    "comment", "Without JWT/passw only localhost is allowed",
                    "username", username,
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
                "username",     "%s", username,
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
                "username",     "%s", username,
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
                "username",     "%s", username,
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

    /*----------------------------------*
     *  Let it in if there's no treedb
     *----------------------------------*/
    if(!priv->gobj_treedb) {
        /*--------------------------------------*
         *  HACK save username in src,
         *  some entry gate like (IEvent_srv)
         *--------------------------------------*/
        gobj_write_str_attr(src, "__username__", username);
        gobj_write_str_attr(src, "__session_id__", "");

        json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s, s:s, s:{s:[]}}",
            "result", 0,
            "comment", comment,
            "username", username,
            "session_id", "",
            "dst_service", dst_service,
            "services_roles", dst_service
        );
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw)
        return jn_resp;
    }

    /*------------------------------*
     *      yuneta
     *------------------------------*/
    if(yuneta_by_local_ip) {
        /*--------------------------------------*
         *  HACK save username in src,
         *  some entry gate like (IEvent_srv)
         *--------------------------------------*/
        gobj_write_str_attr(src, "__username__", username);
        gobj_write_str_attr(src, "__session_id__", "");

        /*
         *  Local trusted yuneta: resolve its REAL roles from treedb (the same
         *  role filter as any user), not a hardcoded empty set. The seed yuneta
         *  user holds the root role (service="*"), so it becomes a superuser and
         *  the ievent gate lets it reach any service (e.g. __yuno__). Guarantee
         *  at least its own dst_service so the local escape hatch survives even
         *  if the user record carries no explicit role for it.
         */
        BOOL superuser = FALSE;
        json_t *services_roles = get_user_roles(
            gobj,
            gobj_yuno_realm_id(),
            dst_service,
            username,
            kw,
            &superuser
        );
        kw_get_list(gobj, services_roles, dst_service, json_array(), KW_CREATE);

        json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s, s:s, s:O, s:b}",
            "result", 0,
            "comment", comment,
            "username", username,
            "session_id", "",
            "dst_service", dst_service,
            "services_roles", services_roles,
            "superuser", superuser
        );
        JSON_DECREF(services_roles);
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
            "username",     "%s", username,
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
    BOOL superuser = FALSE;
    json_t *services_roles = get_user_roles(
        gobj,
        gobj_yuno_realm_id(),
        dst_service,
        username,
        kw,
        &superuser
    );

    if(!kw_has_key(services_roles, dst_service)) {
        /*
         *  No authorized in dst service
         */
        gobj_log_warning(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_AUTH,
            "msg",              "%s", "User has not authz in service",
            "comment",          "%s", comment,
            "username",         "%s", username,
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
        -1,
        KW_REQUIRED
    );

    // TODO if max_sessions is 0 then get the max_sessions of parent role

    /*
     *  If max_sessions continue being 0 get the value of this gclass
     */
    if(max_sessions <= 0) {
        max_sessions = priv->max_sessions_per_user;
    }

    json_t *sessions = kw_get_dict(gobj, user, "__sessions", 0, KW_REQUIRED);
    if(!sessions) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "__sessions NULL",
            "username",     "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
    }

    json_t *session;
    if(max_sessions > 0) {
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
                "max_sessions", "%d", max_sessions,
                "username",     "%s", username,
                "service",      "%s", dst_service,
                "session",      "%j", session,
                NULL
            );
            gobj_send_event(prev_channel_gobj, EV_DROP, 0, gobj);
            json_object_del(sessions, k);
        }
    }

    /*----------------------------------------------------------------------*
     *  HACK save username,session_id,jwt_payload in src
     *  some entry gate like (IEvent_srv)
     *----------------------------------------------------------------------*/
    gobj_write_str_attr(src, "__username__", username);
    gobj_write_str_attr(src, "__session_id__", session_id);
    if(jwt_payload) {
        gobj_write_json_attr(src, "jwt_payload", jwt_payload);
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
        "msg",          "%s", "Login",
        "username",     "%s", username,
        "session_id",   "%s", session_id,
        "peername",     "%s", peername,
        "service",      "%s", dst_service,
        "roles",        "%j", services_roles,
        NULL
    );

    json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s, s:s, s:O, s:b}",
        "result", 0,
        "comment", "User authenticated",
        "username", username,
        "session_id", session_id,
        "dst_service", dst_service,
        "services_roles", services_roles,
        "superuser", superuser
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
     *      Optional password
     *  KC/IdP-authenticated users have no local password (credentials
     *  null) — auth is by JWT.  Only build credentials when a password is
     *  explicitly given; otherwise create the user password-less, the same
     *  way register-idp-user and the initial_load users do.
     *-----------------------------*/
    const char *password = kw_get_str(gobj, kw, "password", "", 0);
    if(!empty_string(password)) {
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
    }

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

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_set_max_sessions(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *username = kw_get_str(gobj, kw, "username", "", 0);
    int max_sessions = (int)kw_get_int(gobj, kw, "max_sessions", 0, KW_WILD_NUMBER);

    const char *dst;
    if(empty_string(username)) {
        dst = gobj_short_name(gobj);
        gobj_write_integer_attr(gobj, "max_sessions_per_user", max_sessions);
        gobj_save_persistent_attrs(gobj, json_string("max_sessions_per_user"));

    } else {
        dst = username;
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
                json_sprintf("User not found: %s", username),
                0,
                0,
                kw  // owned
            );
        }

        json_object_set_new(user, "max_sessions", json_integer(max_sessions));

        json_decref(gobj_update_node(
            priv->gobj_treedb,
            "users",
            user,
            json_pack("{s:b}",
                "with_metadata", 1
            ),
            src
        ));
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Set max_sessions: %d in %s", max_sessions, dst),
        0,
        0,
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
            "msgset",           "%s", MSGSET_CONFIGURATION,
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
            "msgset",           "%s", MSGSET_INTERNAL,
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
            "msgset",           "%s", MSGSET_INTERNAL,
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
            "msgset",           "%s", MSGSET_INTERNAL,
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
            "msgset",           "%s", MSGSET_INTERNAL,
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
            "msgset",           "%s", MSGSET_CONFIGURATION,
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
            /* jwt_checker_verify2 now fails closed: a NULL return means this
             * checker rejected the token (the verdict moved into the return
             * value). Surface this checker's reason so the caller gets a
             * specific error instead of the generic default, then keep trying
             * any other configured issuers. */
            if(jwt_checker->error) {
                *status = jwt_checker_error_msg(jwt_checker);
            }
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
// PRIVATE json_t *identify_system_user(
//     hgobj gobj,
//     const char **username,
//     BOOL include_groups,
//     BOOL verbose
// )
// {
//     PRIVATE_DATA *priv = gobj_priv_data(gobj);
//
//     json_t *user = gobj_get_node(
//         priv->gobj_treedb,
//         "users",
//         json_pack("{s:s, s:b}",
//             "id", *username,
//             "disabled", 0
//         ),
//         json_pack("{s:b}",
//             "with_metadata", 1
//         ),
//         gobj
//     );
//     if(user) {
//         return user;
//     }
//
//     if(include_groups) {
//         /*-------------------------------*
//          *  HACK user's group is valid
//          *-------------------------------*/
//         static gid_t groups[30]; // HACK to use outside
//         int ngroups = sizeof(groups)/sizeof(groups[0]);
//         getgrouplist(*username, 0, groups, &ngroups);
//         for(int i=0; i<ngroups; i++) {
//             struct group *gr = getgrgid(groups[i]);
//             if(gr) {
//                 user = gobj_get_node(
//                     priv->gobj_treedb,
//                     "users",
//                     json_pack("{s:s}", "id", gr->gr_name),
//                     json_pack("{s:b}",
//                         "with_metadata", 1
//                     ),
//                     gobj
//                 );
//                 if(user) {
//                     gobj_log_warning(gobj, 0,
//                         "function",     "%s", __FUNCTION__,
//                         "msgset",       "%s", MSGSET_INFO,
//                         "msg",          "%s", "Using groupname instead of username",
//                         "username",     "%s", *username,
//                         "groupname",    "%s", gr->gr_name,
//                         NULL
//                     );
//                     *username = gr->gr_name;
//                     return user;
//                 }
//             }
//         }
//     }
//
//     if(verbose) {
//         gobj_log_warning(gobj, 0,
//             "function",     "%s", __FUNCTION__,
//             "msgset",       "%s", MSGSET_INFO,
//             "msg",          "%s", "username not found in system",
//             "username",     "%s", *username,
//             NULL
//         );
//     }
//     return 0; // username as user or group not found
// }

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
    /* OpenSSL preferred when both backends are enabled */
    if(RAND_bytes(salt, (int)salt_len) != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "RAND_bytes() FAILED",
            NULL
        );
        return -1;
    }
#elif defined(CONFIG_HAVE_MBEDTLS)
    if(psa_generate_random(salt, salt_len) != PSA_SUCCESS) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "psa_generate_random() FAILED",
            NULL
        );
        return -1;
    }
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
    /* OpenSSL preferred when both backends are enabled */

    EVP_MD *md = EVP_MD_fetch(NULL, digest_name, NULL);
    if(!md) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "PKCS5_PBKDF2_HMAC() failed",
            "digest",       "%s", digest_name,
            NULL
        );
        ret = -1;
    }

    EVP_MD_free(md);
    ret = md_size;

#elif defined(CONFIG_HAVE_MBEDTLS)
    /* Convert lowercase digest name to mbedtls_md_type_t */
    char upper_name[32];
    size_t ni;
    for(ni = 0; ni < sizeof(upper_name) - 1 && digest_name[ni]; ni++) {
        upper_name[ni] = (char)((digest_name[ni] >= 'a' && digest_name[ni] <= 'z')
                                ? digest_name[ni] - 32 : digest_name[ni]);
    }
    upper_name[ni] = '\0';

    mbedtls_md_type_t pmd_type;
    if(strcmp(upper_name, "SHA256") == 0 || strcmp(upper_name, "SHA-256") == 0)
        pmd_type = MBEDTLS_MD_SHA256;
    else if(strcmp(upper_name, "SHA384") == 0 || strcmp(upper_name, "SHA-384") == 0)
        pmd_type = MBEDTLS_MD_SHA384;
    else if(strcmp(upper_name, "SHA512") == 0 || strcmp(upper_name, "SHA-512") == 0)
        pmd_type = MBEDTLS_MD_SHA512;
    else if(strcmp(upper_name, "SHA1") == 0 || strcmp(upper_name, "SHA-1") == 0)
        pmd_type = MBEDTLS_MD_SHA1;
    else if(strcmp(upper_name, "SHA3-256") == 0)
        pmd_type = MBEDTLS_MD_SHA3_256;
    else if(strcmp(upper_name, "SHA3-384") == 0)
        pmd_type = MBEDTLS_MD_SHA3_384;
    else if(strcmp(upper_name, "SHA3-512") == 0)
        pmd_type = MBEDTLS_MD_SHA3_512;
    else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Unsupported digest",
            "digest",       "%s", digest_name,
            NULL
        );
        return -1;
    }

    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(pmd_type);
    if(!md_info) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Unsupported digest",
            "digest",       "%s", digest_name,
            NULL
        );
        return -1;
    }

    int md_size = (int)mbedtls_md_get_size(md_info);
    if((size_t)md_size > out_len) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "out_key size too small",
            "digest",       "%s", digest_name,
            "md_size",      "%d", md_size,
            "out_len",      "%d", (int)out_len,
            NULL
        );
        return -1;
    }

    if(mbedtls_pkcs5_pbkdf2_hmac_ext(mbedtls_md_get_type(md_info),
        (const unsigned char *)password, strlen(password),
        salt, salt_len,
        iterations,
        (uint32_t)md_size, out_key) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "mbedtls_pkcs5_pbkdf2_hmac_ext() failed",
            "digest",       "%s", digest_name,
            NULL
        );
        return -1;
    }

    ret = md_size;

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
            "msgset",       "%s", MSGSET_INTERNAL,
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
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "pbkdf2_any() failed",
            "digest",       "%s", digest,
            NULL
        );
        return NULL;
    }

    gbuffer_t *gbuf_hash = gbuffer_binary_to_base64((const char *)hash, hash_len);
    gbuffer_t *gbuf_salt = gbuffer_binary_to_base64((const char *)salt, sizeof(salt));
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
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "pbkdf2_any() failed",
            "digest",       "%s", digest,
            NULL
        );
        return -1;
    }
    if(hash_len != expected_len) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
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
    gbuffer_t *gbuf_hash = gbuffer_base64_to_binary((const char *)hash_b64, strlen(hash_b64));
    gbuffer_t *gbuf_salt = gbuffer_base64_to_binary((const char *)salt_b64, strlen(salt_b64));
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
            "msgset",       "%s", MSGSET_AUTH,
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
            "msgset",       "%s", MSGSET_AUTH,
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
        "msgset",       "%s", MSGSET_AUTH,
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
/***************************************************************************
 *  TRUE if the role is a wildcard-service (root) role effective in this realm:
 *  service="*" (reaches any service) and realm "*" or the current realm. This
 *  is what makes a channel a superuser for the per-service routing gate.
 ***************************************************************************/
PRIVATE BOOL role_is_wildcard_service(hgobj gobj, json_t *role, const char *dst_realm_id)
{
    const char *r_service = kw_get_str(gobj, role, "service", "", 0);
    if(strcmp(r_service, "*")!=0) {
        return FALSE;
    }
    const char *r_realm = kw_get_str(gobj, role, "realm_id", "", 0);
    if(strcmp(r_realm, "*")==0 || strcmp(r_realm, dst_realm_id)==0) {
        return TRUE;
    }
    return FALSE;
}

PRIVATE json_t *get_user_roles(
    hgobj gobj,
    const char *dst_realm_id,
    const char *dst_service,
    const char *username,
    json_t *kw,         // not owned
    BOOL *superuser     // out, optional: set TRUE if user holds a service="*" role
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

        if(superuser && !*superuser && role_is_wildcard_service(gobj, role, dst_realm_id)) {
            *superuser = TRUE;
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
            if(superuser && !*superuser && role_is_wildcard_service(gobj, child, dst_realm_id)) {
                *superuser = TRUE;
            }
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
            "msgset",       "%s", MSGSET_TREEDB,
            "msg",          "%s", "User not found",
            "username"      "%s", username,
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
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_shutdowning()) {
        KW_DECREF(kw)
        return 0;
    }

    const char *peername;
    if(gobj_has_bottom_attr(src, "peername")) {
        peername = gobj_read_str_attr(src, "peername");
    } else {
        peername = kw_get_str(gobj, kw, "peername", "", 0);
    }

    /*--------------------------------------------*
     *  Add logout user
     *--------------------------------------------*/
    if(priv->gobj_treedb) { // Si han pasado a pause es 0
        const char *session_id = gobj_read_str_attr(src, "__session_id__");
        if(empty_string(session_id)) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_PARAMETER,
                "msg",              "%s", "__session_id__ not found",
                "src",              "%s", gobj_full_name(src),
                NULL
            );
        }
        const char *username = gobj_read_str_attr(src, "__username__");

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
                "msgset",       "%s", MSGSET_INTERNAL,
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
                "username",     "%s", username,
                "session_id",   "%s", session_id,
                "peername",     "%s", peername,
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
PRIVATE int ac_create_user(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
    BOOL new_user = user?FALSE:TRUE;    // node found => NOT new (was inverted)
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
PRIVATE int ac_reject_user(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
            "msgset",       "%s", MSGSET_INTERNAL,
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

                    /***************************
                     *  IdP (Keycloak) provisioning
                     ***************************/




/***************************************************************************
 *  Lazily create the shared outbound HTTP client to Keycloak, reading the
 *  runtime-configured kc_base_url.  FALSE if KC is not configured yet.
 ***************************************************************************/
PRIVATE BOOL ensure_kc_client(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_kc) {
        return TRUE;
    }
    const char *kc_base_url = gobj_read_str_attr(gobj, "kc_base_url");
    if(empty_string(kc_base_url)) {
        return FALSE;
    }
    json_t *jn_crypto = gobj_read_json_attr(gobj, "kc_crypto");
    priv->gobj_kc = gobj_create(
        priv->kc_name, C_PROT_HTTP_CL,
        json_pack("{s:s}", "url", kc_base_url), gobj);
    gobj_set_manual_start(priv->gobj_kc, TRUE);
    gobj_set_bottom_gobj(
        priv->gobj_kc,
        gobj_create_pure_child(
            priv->kc_name, C_TCP,
            json_pack("{s:s, s:O, s:i}",
                "url", kc_base_url, "crypto", jn_crypto,
                "timeout_between_connections", 100),
            priv->gobj_kc));
    return TRUE;
}

/***************************************************************************
 *  Snapshot of the Keycloak admin config with the secret masked.
 ***************************************************************************/
PRIVATE json_t *kc_config_snapshot(hgobj gobj)
{
    const char *secret = gobj_read_str_attr(gobj, "kc_admin_client_secret");
    json_t *jn = json_object();
    json_object_set_new(jn, "kc_base_url",
        json_string(gobj_read_str_attr(gobj, "kc_base_url")));
    json_object_set_new(jn, "kc_realm",
        json_string(gobj_read_str_attr(gobj, "kc_realm")));
    json_object_set_new(jn, "kc_admin_client_id",
        json_string(gobj_read_str_attr(gobj, "kc_admin_client_id")));
    json_object_set_new(jn, "kc_redirect_uri",
        json_string(gobj_read_str_attr(gobj, "kc_redirect_uri")));
    json_object_set_new(jn, "kc_email_client_id",
        json_string(gobj_read_str_attr(gobj, "kc_email_client_id")));
    json_object_set_new(jn, "kc_admin_client_secret",
        json_string(empty_string(secret)? "" : "********"));
    return jn;
}

/***************************************************************************
 *  Deliver the deferred answer of a register-idp-user command.  The
 *  requester channel is re-resolved by name so a client that disconnected
 *  mid-flight is never dereferenced (mirrors the agent's async answers).
 ***************************************************************************/
PRIVATE void kc_answer(hgobj gobj, KC_PENDING *p, int result,
    json_t *jn_comment, json_t *jn_data, const char *warning)
{
    if(p->answered) {
        JSON_DECREF(jn_comment)
        JSON_DECREF(jn_data)
        return;
    }
    p->answered = TRUE;

    if(!jn_data) {
        jn_data = json_object();
    }
    if(!empty_string(warning) && json_is_object(jn_data)) {
        json_object_set_new(jn_data, "warning", json_string(warning));
    }

    hgobj gate = gobj_find_service(p->req_service, FALSE);
    hgobj requester = gate? gobj_child_by_name(gate, p->req_channel) : NULL;
    if(!requester) {
        JSON_DECREF(jn_comment)
        JSON_DECREF(jn_data)
        return;
    }
    json_t *webix = msg_iev_build_response(
        gobj, result, jn_comment, 0, jn_data, json_incref(p->kw_request));
    json_t *iev = iev_create(gobj, EV_MT_COMMAND_ANSWER, webix);
    gobj_send_event(requester, EV_SEND_IEV, iev, gobj);
}

/***************************************************************************
 *  Job 1 action: request an admin access_token (client_credentials).
 ***************************************************************************/
PRIVATE json_t *kc_get_token(hgobj gobj, const char *lmethod, json_t *kw, hgobj src_task)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char resource[256];
    snprintf(resource, sizeof(resource),
        "/realms/%s/protocol/openid-connect/token",
        gobj_read_str_attr(gobj, "kc_realm"));

    json_t *jn_headers = json_pack("{s:s}",
        "Content-Type", "application/x-www-form-urlencoded");
    json_t *jn_data = json_pack("{s:s, s:s, s:s}",
        "grant_type",    "client_credentials",
        "client_id",     gobj_read_str_attr(gobj, "kc_admin_client_id"),
        "client_secret", gobj_read_str_attr(gobj, "kc_admin_client_secret"));
    json_t *query = json_pack("{s:s, s:s, s:s, s:o, s:o}",
        "method", "POST", "resource", resource, "query", "",
        "headers", jn_headers, "data", jn_data);
    gobj_send_event(priv->gobj_kc, EV_SEND_MESSAGE, query, gobj);

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *  Job 1 result: cache the admin token (or answer error and stop).
 ***************************************************************************/
PRIVATE json_t *kc_save_token(hgobj gobj, const char *lmethod, json_t *kw, hgobj src_task)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    KC_PENDING *p = dl_first(&priv->dl_kc_pending);

    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);
    if(status != 200) {
        if(p) {
            kc_answer(gobj, p, -1,
                json_sprintf("Keycloak admin authentication failed (status %d)", status),
                json_pack("{s:s}", "error_code",
                    (status==401||status==403)? "kc_token_refused" : "kc_unavailable"),
                NULL);
        }
        KW_DECREF(kw)
        STOP_TASK()
    }

    json_t *jn_body = kw_get_dict(gobj, kw, "body", NULL, 0);
    const char *access_token = jn_body? kw_get_str(gobj, jn_body, "access_token", "", 0) : "";
    int64_t expires_in = jn_body? kw_get_int(gobj, jn_body, "expires_in", 60, 0) : 60;
    if(empty_string(access_token)) {
        if(p) {
            kc_answer(gobj, p, -1,
                json_sprintf("Keycloak admin authentication: empty token"),
                json_pack("{s:s}", "error_code", "kc_unavailable"), NULL);
        }
        KW_DECREF(kw)
        STOP_TASK()
    }

    snprintf(priv->kc_token, sizeof(priv->kc_token), "%s", access_token);
    priv->kc_token_expiry = time(NULL) + (time_t)expires_in;

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *  Job 2 action: POST /admin/realms/<realm>/users.
 ***************************************************************************/
PRIVATE json_t *kc_create_user(hgobj gobj, const char *lmethod, json_t *kw, hgobj src_task)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    KC_PENDING *p = dl_first(&priv->dl_kc_pending);
    if(!p) {
        KW_DECREF(kw)
        STOP_TASK()
    }

    char resource[256];
    snprintf(resource, sizeof(resource), "/admin/realms/%s/users",
        gobj_read_str_attr(gobj, "kc_realm"));
    char bearer[8300];
    snprintf(bearer, sizeof(bearer), "Bearer %s", priv->kc_token);

    json_t *jn_headers = json_pack("{s:s, s:s}",
        "Content-Type", "application/json", "Authorization", bearer);
    json_t *jn_data = json_pack(
        "{s:s, s:s, s:b, s:s, s:s, s:b, s:[s,s]}",
        "username",        p->email,
        "email",           p->email,
        "enabled",         1,
        "firstName",       p->first_name,
        "lastName",        p->last_name,
        "emailVerified",   0,
        "requiredActions", "UPDATE_PASSWORD", "VERIFY_EMAIL");
    json_t *query = json_pack("{s:s, s:s, s:s, s:o, s:o}",
        "method", "POST", "resource", resource, "query", "",
        "headers", jn_headers, "data", jn_data);
    gobj_send_event(priv->gobj_kc, EV_SEND_MESSAGE, query, gobj);

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *  Job 2 result: on 201, read user id from Location + dual-write the local
 *  authz user; map every other status to a stable error_code.
 ***************************************************************************/
PRIVATE json_t *kc_create_user_result(hgobj gobj, const char *lmethod, json_t *kw, hgobj src_task)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    KC_PENDING *p = dl_first(&priv->dl_kc_pending);

    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);

    if(status == 401) {
        priv->kc_token[0] = 0;
        priv->kc_token_expiry = 0;
        if(p) {
            kc_answer(gobj, p, -1, json_sprintf("Keycloak rejected the admin token"),
                json_pack("{s:s}", "error_code", "kc_token_refused"), NULL);
        }
        KW_DECREF(kw)
        STOP_TASK()
    }
    if(status == 409) {
        if(p) {
            kc_answer(gobj, p, -1, json_sprintf("User already exists: %s", p->email),
                json_pack("{s:s}", "error_code", "user_already_exists"), NULL);
        }
        KW_DECREF(kw)
        STOP_TASK()
    }
    if(status != 201) {
        json_t *jn_body = kw_get_dict(gobj, kw, "body", NULL, 0);
        const char *kc_msg = jn_body? kw_get_str(gobj, jn_body, "errorMessage", "", 0) : "";
        const char *code = (status>=400 && status<500)? "kc_validation_error" : "kc_unavailable";
        if(p) {
            kc_answer(gobj, p, -1,
                empty_string(kc_msg)?
                    json_sprintf("Keycloak create-user failed (status %d)", status) :
                    json_sprintf("%s", kc_msg),
                json_pack("{s:s}", "error_code", code), NULL);
        }
        KW_DECREF(kw)
        STOP_TASK()
    }

    /*  201 Created — user id is the last path segment of Location. */
    if(p) {
        json_t *jn_headers = kw_get_dict(gobj, kw, "headers", NULL, 0);
        const char *location = jn_headers? kw_get_str(gobj, jn_headers, "LOCATION", "", 0) : "";
        const char *slash = !empty_string(location)? strrchr(location, '/') : NULL;
        if(slash && slash[1]) {
            snprintf(p->user_id, sizeof(p->user_id), "%s", slash + 1);
        }

        /*  Dual-write the local authz user (id=email, chosen role, NO local
         *  credentials — Keycloak is the IdP).  EV_ADD_USER to ourselves
         *  creates the node and emits EV_AUTHZ_USER_NEW. */
        json_t *jn_add = json_pack("{s:s, s:b}", "username", p->email, "disabled", 0);
        if(!empty_string(p->role)) {
            json_object_set_new(jn_add, "role",
                json_sprintf("roles^%s^users", p->role));
        }
        gobj_send_event(gobj, EV_ADD_USER, jn_add, gobj);
        json_t *u = gobj_get_node(priv->gobj_treedb, "users",
            json_pack("{s:s}", "id", p->email), 0, gobj);
        p->authz_ok = u? TRUE : FALSE;
        JSON_DECREF(u)
    }

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *  Job 3 action: PUT execute-actions-email (set-password invite).
 ***************************************************************************/
PRIVATE json_t *kc_send_email(hgobj gobj, const char *lmethod, json_t *kw, hgobj src_task)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    KC_PENDING *p = dl_first(&priv->dl_kc_pending);
    if(!p || empty_string(p->user_id)) {
        KW_DECREF(kw)
        STOP_TASK()
    }

    /*  Query params go in the resource: C_PROT_HTTP_CL only appends the
     *  `query` field for x-www-form-urlencoded, not for JSON bodies. */
    char resource[768];
    snprintf(resource, sizeof(resource),
        "/admin/realms/%s/users/%s/execute-actions-email"
        "?client_id=%s&redirect_uri=%s&lifespan=43200",
        gobj_read_str_attr(gobj, "kc_realm"),
        p->user_id,
        gobj_read_str_attr(gobj, "kc_email_client_id"),
        gobj_read_str_attr(gobj, "kc_redirect_uri"));
    char bearer[8300];
    snprintf(bearer, sizeof(bearer), "Bearer %s", priv->kc_token);

    json_t *jn_headers = json_pack("{s:s, s:s}",
        "Content-Type", "application/json", "Authorization", bearer);
    json_t *jn_data = json_pack("[s,s]", "UPDATE_PASSWORD", "VERIFY_EMAIL");
    json_t *query = json_pack("{s:s, s:s, s:s, s:o, s:o}",
        "method", "PUT", "resource", resource, "query", "",
        "headers", jn_headers, "data", jn_data);
    gobj_send_event(priv->gobj_kc, EV_SEND_MESSAGE, query, gobj);

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *  Job 3 result (final): answer the caller.  The user already exists in
 *  Keycloak + authz, so this is always result 0 — a failed invite or a
 *  missed authz write is reported as a non-fatal `warning`.
 ***************************************************************************/
PRIVATE json_t *kc_send_email_result(hgobj gobj, const char *lmethod, json_t *kw, hgobj src_task)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    KC_PENDING *p = dl_first(&priv->dl_kc_pending);

    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);
    if(p) {
        const char *warning = NULL;
        if(!p->authz_ok) {
            warning = "authz_write_failed";
        } else if(!(status >= 200 && status < 300)) {
            warning = "email_send_failed";
        }
        kc_answer(gobj, p, 0,
            json_sprintf("User registered: %s", p->email),
            json_pack("{s:s, s:s}", "id", p->user_id, "email", p->email),
            warning);
    }

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *  Launch the next queued register-idp-user (serialized through the one
 *  shared KC connection).  Token job is skipped when a fresh token cached.
 ***************************************************************************/
PRIVATE void process_next_kc(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->kc_processing || dl_size(&priv->dl_kc_pending) == 0) {
        return;
    }
    if(!ensure_kc_client(gobj)) {
        /*  Not configured yet (no kc_base_url): drain with errors. */
        KC_PENDING *p = dl_first(&priv->dl_kc_pending);
        dl_delete(&priv->dl_kc_pending, p, 0);
        kc_answer(gobj, p, -1,
            json_sprintf("Keycloak admin is not configured (run set-kc-config)"),
            json_pack("{s:s}", "error_code", "kc_unavailable"), NULL);
        JSON_DECREF(p->kw_request)
        GBMEM_FREE(p)
        process_next_kc(gobj);
        return;
    }

    priv->kc_processing = TRUE;

    BOOL have_token = !empty_string(priv->kc_token) &&
        time(NULL) < priv->kc_token_expiry - KC_TOKEN_SKEW_S;

    json_int_t timeout_ms = gobj_read_integer_attr(gobj, "kc_timeout_ms");
    if(timeout_ms <= 0) {
        timeout_ms = 30000;
    }

    json_t *jobs = json_array();
    if(!have_token) {
        json_array_append_new(jobs, json_pack("{s:s, s:s, s:I}",
            "exec_action", "kc_get_token", "exec_result", "kc_save_token",
            "exec_timeout", timeout_ms));
    }
    json_array_append_new(jobs, json_pack("{s:s, s:s, s:I}",
        "exec_action", "kc_create_user", "exec_result", "kc_create_user_result",
        "exec_timeout", timeout_ms));
    json_array_append_new(jobs, json_pack("{s:s, s:s, s:I}",
        "exec_action", "kc_send_email", "exec_result", "kc_send_email_result",
        "exec_timeout", timeout_ms));

    json_t *kw_task = json_pack("{s:I, s:I, s:o, s:o}",
        "gobj_jobs",    (json_int_t)(uintptr_t)gobj,
        "gobj_results", (json_int_t)(uintptr_t)priv->gobj_kc,
        "output_data",  json_object(),
        "jobs",         jobs);

    priv->gobj_kc_task = gobj_create(priv->kc_name, C_TASK, kw_task, gobj);
    gobj_subscribe_event(priv->gobj_kc_task, EV_END_TASK, NULL, gobj);
    gobj_set_volatil(priv->gobj_kc_task, TRUE);

    gobj_start(priv->gobj_kc_task);
    if(!gobj_is_running(priv->gobj_kc)) {
        gobj_start(priv->gobj_kc);
    }
}

/***************************************************************************
 *  cmd set-kc-config — write & persist the provided kc_* attrs.
 ***************************************************************************/
PRIVATE json_t *cmd_set_kc_config(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_user_has_authz(gobj, "configure-kc", kw_incref(kw), src)) {
        return msg_iev_build_response(gobj, -403,
            json_sprintf("No permission to 'configure-kc'"),
            0, json_pack("{s:s}", "error_code", "no_permission"), kw);
    }

    static const char *cfg_keys[] = {
        "kc_base_url", "kc_realm", "kc_admin_client_id",
        "kc_admin_client_secret", "kc_redirect_uri", "kc_email_client_id", 0
    };
    json_t *jn_save = json_array();
    for(int i = 0; cfg_keys[i]; i++) {
        const char *v = kw_get_str(gobj, kw, cfg_keys[i], 0, 0);
        if(v) {
            gobj_write_str_attr(gobj, cfg_keys[i], v);
            json_array_append_new(jn_save, json_string(cfg_keys[i]));
        }
    }
    if(json_array_size(jn_save) == 0) {
        JSON_DECREF(jn_save)
        return msg_iev_build_response(gobj, -1,
            json_sprintf("No kc_* parameter provided"), 0, 0, kw);
    }
    gobj_save_persistent_attrs(gobj, jn_save);  // owned

    /*  Drop the cached client + token so the new endpoint/credentials take
     *  effect on the next register-idp-user (skip if a round-trip is live). */
    if(priv->gobj_kc && !priv->kc_processing) {
        gobj_destroy(priv->gobj_kc);
        priv->gobj_kc = 0;
    }
    priv->kc_token[0] = 0;
    priv->kc_token_expiry = 0;

    return msg_iev_build_response(gobj, 0,
        json_sprintf("Keycloak admin config saved"), 0,
        kc_config_snapshot(gobj), kw);
}

/***************************************************************************
 *  cmd view-kc-config — show the persisted config (secret masked).
 ***************************************************************************/
PRIVATE json_t *cmd_view_kc_config(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    if(!gobj_user_has_authz(gobj, "configure-kc", kw_incref(kw), src)) {
        return msg_iev_build_response(gobj, -403,
            json_sprintf("No permission to 'configure-kc'"),
            0, json_pack("{s:s}", "error_code", "no_permission"), kw);
    }
    return msg_iev_build_response(gobj, 0, 0, 0, kc_config_snapshot(gobj), kw);
}

/***************************************************************************
 *  cmd register-idp-user — create the user in Keycloak + local authz
 *  (deferred async answer).
 ***************************************************************************/
PRIVATE json_t *cmd_register_idp_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_user_has_authz(gobj, "register-idp-user", kw_incref(kw), src)) {
        return msg_iev_build_response(gobj, -403,
            json_sprintf("No permission to 'register-idp-user'"),
            0, json_pack("{s:s}", "error_code", "no_permission"), kw);
    }

    const char *email = kw_get_str(gobj, kw, "email", "", 0);
    if(empty_string(email) || !strchr(email, '@')) {
        return msg_iev_build_response(gobj, -1, json_sprintf("Invalid email"),
            0, json_pack("{s:s}", "error_code", "invalid_email"), kw);
    }
    const char *role = kw_get_str(gobj, kw, "role", "", 0);
    if(!empty_string(role)) {
        json_t *r = gobj_get_node(priv->gobj_treedb, "roles",
            json_pack("{s:s}", "id", role), 0, gobj);
        BOOL ok = r? TRUE : FALSE;
        JSON_DECREF(r)
        if(!ok) {
            return msg_iev_build_response(gobj, -1,
                json_sprintf("Unknown role: %s", role),
                0, json_pack("{s:s}", "error_code", "unknown_role"), kw);
        }
    }
    if((int)dl_size(&priv->dl_kc_pending) >= KC_MAX_PENDING) {
        return msg_iev_build_response(gobj, -1,
            json_sprintf("Server busy, retry in a moment"),
            0, json_pack("{s:s}", "error_code", "kc_busy"), kw);
    }

    KC_PENDING *p = GBMEM_MALLOC(sizeof(KC_PENDING));
    if(!p) {
        return msg_iev_build_response(gobj, -1, json_sprintf("Out of memory"),
            0, json_pack("{s:s}", "error_code", "kc_unavailable"), kw);
    }
    memset(p, 0, sizeof(*p));
    snprintf(p->email,      sizeof(p->email),      "%s", email);
    snprintf(p->first_name, sizeof(p->first_name), "%s", kw_get_str(gobj, kw, "firstName", "", 0));
    snprintf(p->last_name,  sizeof(p->last_name),  "%s", kw_get_str(gobj, kw, "lastName", "", 0));
    snprintf(p->role,       sizeof(p->role),       "%s", role);

    json_t *iev_stack = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, FALSE);
    snprintf(p->req_service, sizeof(p->req_service), "%s",
        iev_stack? kw_get_str(gobj, iev_stack, "input_service", "", 0) : "");
    snprintf(p->req_channel, sizeof(p->req_channel), "%s",
        iev_stack? kw_get_str(gobj, iev_stack, "input_channel", "", 0) : "");

    p->kw_request = json_incref(kw);
    dl_add(&priv->dl_kc_pending, p);
    process_next_kc(gobj);

    KW_DECREF(kw)
    return 0;  /* async — answered later by kc_answer */
}

/***************************************************************************
 *  A register-idp-user task finished: answer if not already, free the
 *  pending, drain the next queued request.
 ***************************************************************************/
PRIVATE int ac_end_task(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_kc_task = NULL;  /* ac_stopped destroys the volatile task */
    priv->kc_processing = FALSE;

    int result = (int)kw_get_int(gobj, kw, "result", 0, 0);

    KC_PENDING *p = dl_first(&priv->dl_kc_pending);
    if(p) {
        dl_delete(&priv->dl_kc_pending, p, 0);
        if(!p->answered) {
            kc_answer(gobj, p, -1,
                json_sprintf(result==-2?
                    "Keycloak did not respond in time" :
                    "Keycloak register-idp-user failed"),
                json_pack("{s:s}", "error_code",
                    result==-2? "kc_timeout" : "kc_unavailable"),
                NULL);
        }
        JSON_DECREF(p->kw_request)
        GBMEM_FREE(p)
    }

    if(result == -2 && priv->gobj_kc &&
            gobj_read_bool_attr(priv->gobj_kc, "connected")) {
        gobj_stop(priv->gobj_kc);
    }

    KW_DECREF(kw)
    process_next_kc(gobj);
    return 0;
}

/***************************************************************************
 *  Destroy the volatile KC task once it has stopped.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
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

/*------------------------*
 *      Local methods (register-idp-user C_TASK jobs)
 *------------------------*/
PRIVATE LMETHOD lmt[] = {
    {"kc_get_token",            kc_get_token,           0},
    {"kc_save_token",           kc_save_token,          0},
    {"kc_create_user",          kc_create_user,         0},
    {"kc_create_user_result",   kc_create_user_result,  0},
    {"kc_send_email",           kc_send_email,          0},
    {"kc_send_email_result",    kc_send_email_result,   0},
    {0, 0, 0}
};

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
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
        {EV_END_TASK,             ac_end_task,              0},
        {EV_STOPPED,              ac_stopped,               0},
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
        {EV_END_TASK,           0},
        {EV_STOPPED,            0},
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
        lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
                "msgset",       "%s", MSGSET_INTERNAL,
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

    BOOL allow = FALSE;
    const char *authz_; json_t *jn_allow;
    json_object_foreach(user_authzs, authz_, jn_allow) {
        if(strcmp(authz_, "*")==0 || strcmp(authz_, authz)==0) {
            allow = json_boolean_value(jn_allow)?1:0;
            break;
        }
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(gobj, user_authzs, "user '%s', service: '%s', authz '%s', allow -> %s",
            __username__,
            gobj_short_name(gobj_to_check),
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
