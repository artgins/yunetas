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
// #include "../../libjwt/src/jwt-private.h"

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

PRIVATE int is_yuneta_user(const char *username);

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
PRIVATE json_t *cmd_roles(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_user_roles(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_user_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,  "level",        0,              0,          "level=1: search in bottoms, level=2: search in all childs"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_add_jwk[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "kid",          0,              0,          "Key ID – used to match JWT's kid to the correct key, same as 'iss'"),
SDATAPM (DTP_STRING,    "iss",          0,              0,          "Key ID – used to match JWT's kid to the correct key, same as 'kid'"),
SDATAPM (DTP_STRING,    "kty",          0,              "RSA",      "Key type (e.g. RSA, EC)"),
SDATAPM (DTP_STRING,    "use",          0,              "sig",      "Intended key use (sig = signature, enc = encryption)"),
SDATAPM (DTP_STRING,    "alg",          0,              "RS256",    "Algorithm used with the key (e.g. RS256)"),
SDATAPM (DTP_STRING,    "n",            0,              0,          "RSA modulus, base64url-encoded, same as 'pkey'"),
SDATAPM (DTP_STRING,    "pkey",         0,              0,          "RSA modulus, base64url-encoded, same as 'n'"),
SDATAPM (DTP_STRING,    "e",            0,              "AQAB",     "RSA exponent, usually 'AQAB' (65537), base64url-encoded"),
SDATAPM (DTP_STRING,    "x5c",          0,              0,          "(Optional) X.509 certificate chain"),

SDATAPM (DTP_STRING,    "description",  0,              0,          "Description"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_rm_jwk[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "kid",          0,              0,          "Key ID, same as 'iss'"),
SDATAPM (DTP_STRING,    "iss",          0,              0,          "Issuer, same as 'kid'"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_users[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_JSON,      "filter",       0,              0,          "Filter"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_create_user[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,              0,          "Username"),
SDATAPM (DTP_STRING,    "role",         0,              0,          "Role, format: roles^ROLE^users"),
SDATAPM (DTP_BOOLEAN,   "disabled",     0,              0,          "Disabled"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_enable_user[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,              0,          "Username"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_disable_user[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,              0,          "Username"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_roles[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_JSON,      "filter",       0,              0,          "Filter"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_user_roles[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,              0,          "Username"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_user_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,              0,          "Username"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help, pm_help,        cmd_help,       "Command's help"),
SDATACM (DTP_SCHEMA,    "authzs",           0,      pm_authzs,      cmd_authzs,     "Authorization's help"),

/*-CMD2---type----------name------------flag------------alias---items---------------json_fn-------------description--*/
SDATACM2(DTP_SCHEMA,    "list-jwk",     SDF_AUTHZ_X,    0,      0,              cmd_list_jwk,   "List OAuth2 JWKs"),
SDATACM2(DTP_SCHEMA,    "add-jwk",      SDF_AUTHZ_X,    0,      pm_add_jwk,     cmd_add_jwk,    "Add OAuth2 JWK"),
SDATACM2(DTP_SCHEMA,    "remove-jwk",   SDF_AUTHZ_X,    0,      pm_rm_jwk,      cmd_remove_jwk, "Remove OAuth2 JWK"),
SDATACM2(DTP_SCHEMA,    "users",        SDF_AUTHZ_X,    0,      pm_users,       cmd_users,      "List users"),
SDATACM2(DTP_SCHEMA,    "accesses",     SDF_AUTHZ_X,    0,      pm_users,       cmd_accesses,   "List user accesses"),
SDATACM2(DTP_SCHEMA,    "create-user",  SDF_AUTHZ_X,    0,      pm_create_user, cmd_create_user,"Create or update user (see ROLE format)"),
SDATACM2(DTP_SCHEMA,    "enable-user",  SDF_AUTHZ_X,    0,      pm_enable_user, cmd_enable_user,"Enable user"),
SDATACM2(DTP_SCHEMA,    "disable-user", SDF_AUTHZ_X,    0,      pm_disable_user,cmd_disable_user,"Disable user"),
SDATACM2(DTP_SCHEMA,    "roles",        SDF_AUTHZ_X,    0,      pm_roles,       cmd_roles,      "List roles"),
SDATACM2(DTP_SCHEMA,    "user-roles",   SDF_AUTHZ_X,    0,      pm_user_roles,  cmd_user_roles, "Get roles of user"),
SDATACM2(DTP_SCHEMA,    "user-authzs",  SDF_AUTHZ_X,    0,      pm_user_authzs, cmd_user_authzs,"Get permissions of user"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description---------- */
SDATA (DTP_INTEGER,     "max_sessions_per_user",SDF_PERSIST,    "1",        "Max sessions per user"),
SDATA (DTP_JSON,        "jwks",                 SDF_WR|SDF_PERSIST, "[]",   "JWKS public keys, OLD jwt_public_keys, use the utility keycloak_pkey_to_jwks to create."),
SDATA (DTP_JSON,        "initial_load",         SDF_RD,         "{}",       "Initial data for treedb"),
/*
 *  HACK WARNING 2024-Nov-13: use of "tranger_path" to determine if this instance is master or not.
 *  If tranger_path is empty, then
 *      the class uses yuneta_realm_store_dir() to setup the tranger "authzs" as master TRUE
 *  if it's not empty:
 *      use master as set externally
 */
SDATA (DTP_STRING,      "tranger_path",     SDF_RD,             "",         "Tranger path, internal value (or not)"),
SDATA (DTP_STRING,      "authz_yuno_role",  SDF_RD,             "",         "If tranger_path is empty you can force the yuno_role where build the authz. If authz_yuno_role is empty get it from this yuno."),
SDATA (DTP_BOOLEAN,     "master",           SDF_RD,             "0",        "the master is the only that can write, if tranger_path is empty is set to TRUE internally"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendent value!
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

    /*---------------------------*
     *  Create Timeranger
     *---------------------------*/
    const char *path = gobj_read_str_attr(gobj, "tranger_path");
    BOOL master = gobj_read_bool_attr(gobj, "master");

    if(empty_string(path)) {
        /*--------------------------------------------*
         *  Without path, it must be master (or not)
         *--------------------------------------------*/
        /*
         *  Set the path
         */
        const char *authz_yuno_role = gobj_read_str_attr(gobj, "authz_yuno_role");
        if(empty_string(authz_yuno_role)) {
            authz_yuno_role = gobj_yuno_role();
        }
        char path_[PATH_MAX];
        yuneta_realm_store_dir(
            path_,
            sizeof(path_),
            authz_yuno_role,
            gobj_yuno_realm_owner(),
            gobj_yuno_realm_id(),
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
        const char *topic_name;
        json_t *topic_records;
        json_object_foreach(initial_load, topic_name, topic_records) {
            int idx; json_t *record;
            json_array_foreach(topic_records, idx, record) {
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
            "session_state": "aa4fb7ce-d0c7-48a0-ae92-253ef5a600d2",
            "sid": "aa4fb7ce-d0c7-48a0-ae92-253ef5a600d2",
            "sub": "0a1e5c27-80f1-4225-943a-edfbc204972d",
            "typ": "Bearer"
        }
    Ejemplo de jwt dado por google  {
            "aud": "apps.googleusercontent.com",
            "azp": "apps.googleusercontent.com",
            "email": "xxx@gmail.com",
            "email_verified": TRUE,
            "exp": 1666341427,
            "given_name": "Gins",
            "iat": 1666337827,
            "iss": "https://accounts.google.com",
            "jti": "b2a",
            "name": "Xxx",
            "nbf": 1337527,
            "picture": "",
            "sub": "109408784262322618770"
        }
 ***************************************************************************/
PRIVATE json_t *mt_authenticate(hgobj gobj, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *peername = gobj_read_str_attr(src, "peername");
    const char *jwt= kw_get_str(gobj, kw, "jwt", "", 0);
    const char *username = "";

    /*-----------------------------*
     *  Get destination service
     *-----------------------------*/
    const char *dst_service = kw_get_str(gobj,
        kw,
        "__md_iev__`ievent_gate_stack`0`dst_service",
        "",
        KW_REQUIRED
    );
    if(!gobj_find_service(dst_service, FALSE)) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "Destination service not found",
            "user",         "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
        KW_DECREF(kw)
        return json_pack("{s:i, s:s, s:s}",
            "result", -1,
            "comment", "Destination service not found",
            "service", dst_service
        );
    }

    if(empty_string(jwt)) {
        /*-------------------------------*
         *  Without JWT, check local
         *-------------------------------*/
        if(is_ip_denied(peername)) {
            /*
             *  IP no autorizada sin user/passw, informa
             */
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_AUTH,
                "msg",          "%s", "Ip denied",
                "user",         "%s", username,
                "service",      "%s", dst_service,
                NULL
            );
            KW_DECREF(kw)
            return json_pack("{s:i, s:s}",
                "result", -1,
                "comment", "Ip denied"
            );
        }
        struct passwd *pw = getpwuid(getuid());
        username = pw->pw_name;

        if(is_yuneta_user(username)) {
            username = "yuneta";
        } else {
            json_t *user = identify_system_user(gobj, &username, TRUE, FALSE);
            if(!user) {
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_AUTH,
                    "msg",          "%s", "System user not found or not authorized",
                    "user",         "%s", username,
                    "service",      "%s", dst_service,
                    NULL
                );
                KW_DECREF(kw)
                return json_pack("{s:i, s:s, s:s}",
                    "result", -1,
                    "comment", "System user not found or not authorized",
                    "username", username
                );
            }
            json_decref(user);
        }

        char *comment = "";
        do {
            if(is_ip_allowed(peername)) {
                /*
                 *  IP autorizada sin user/passw, usa logged user
                 */
                comment = "Registered Ip allowed";
                break;
            }

            const char *localhost = "127.0.0.";
            if(strncmp(peername, localhost, strlen(localhost))!=0) {
                /*
                 *  Only localhost is allowed without jwt
                 */
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_AUTH,
                    "msg",          "%s", "Without JWT only localhost is allowed",
                    "user",         "%s", username,
                    "service",      "%s", dst_service,
                    NULL
                );
                KW_DECREF(kw)
                return json_pack("{s:i, s:s}",
                    "result", -1,
                    "comment", "Without JWT only localhost is allowed"
                );
            }
            comment = "Local Ip allowed";
        } while(0);

        /*------------------------------------------------*
         *  HACK guarda username en src (IEvent_srv)
         *------------------------------------------------*/
        gobj_write_str_attr(src, "__username__", username);

        /*
         *  Autorizado
         */
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "LOGIN: Local Ip allowed",
            "user",         "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
        KW_DECREF(kw)
        return json_pack("{s:i, s:s, s:s, s:s, s:{}, s:o}",
            "result", 0,
            "comment", comment,
            "username", username,
            "dst_service", dst_service,
            "services_roles",
            "jwt_payload", json_null()
        );
    }

    /*-------------------------------*
     *      HERE with user JWT
     *-------------------------------*/
    json_t *jwt_payload = NULL;
    const char *status = NULL;

    if(!verify_token(gobj, jwt, &jwt_payload, &status)) {
        char temp[512];
        snprintf(temp, sizeof(temp), "%s", status);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "NO verify_token",
            "status",       "%s", temp,
            "user",         "%s", username,
            "service",      "%s", dst_service,
            "jwt",          "%s", jwt,
            NULL
        );
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw)
        return json_pack("{s:i, s:s, s:s}",
            "result", -1,
            "comment", status,
            "username", username
        );
    }

    /*-------------------------------------------------*
     *  Get username and validate against our system
     *-------------------------------------------------*/
    username = kw_get_str(gobj, jwt_payload, "email", "", KW_REQUIRED);
    BOOL email_verified = kw_get_bool(gobj, jwt_payload, "email_verified", FALSE, KW_REQUIRED);
    if(!email_verified) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "Email not verified",
            "user",         "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw)
        return json_pack("{s:i, s:s, s:s}",
            "result", -1,
            "comment", "Email not verified",
            "username", username
        );
    }
    if(!strchr(username, '@')) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "Username must be an email address",
            "user",         "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw)
        return json_pack("{s:i, s:s, s:s}",
            "result", -1,
            "comment", "Username must be an email address",
            "username", username
        );
    }
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

        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "User not authorized",
            "user",         "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
        json_t *jn_msg = json_pack("{s:i, s:s, s:s}",
            "result", -1,
            "comment", "User not authorized",
            "username", username
        );
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw)
        return jn_msg;
    }

    BOOL disabled = kw_get_bool(gobj, user, "disabled", 0, KW_REQUIRED);
    if(disabled) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "User disabled",
            "user",         "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
        json_t *jn_msg = json_pack("{s:i, s:s, s:s}",
            "result", -1,
            "comment", "User disabled",
            "username", username
        );
        json_decref(user);
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw)
        return jn_msg;
    }

    /*----------------------------------------------------------*
     *  HACK guarda username, jwt_payload en src (IEvent_srv)
     *----------------------------------------------------------*/
    gobj_write_str_attr(src, "__username__", username);
    gobj_write_json_attr(src, "jwt_payload", jwt_payload);

    /*------------------------------*
     *      Save user access
     *------------------------------*/
    add_user_login(gobj, username, jwt_payload, peername);

    /*--------------------------------------------*
     *  Get sessions, check max sessions allowed
     *--------------------------------------------*/
    json_t *sessions = kw_get_dict(gobj, user, "__sessions", 0, KW_REQUIRED);
    if(!sessions) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH_ERROR,
            "msg",          "%s", "__sessions NULL",
            NULL
        );
    }
    json_t *session;
    void *n; const char *k;
    json_object_foreach_safe(sessions, n, k, session) {
        if(json_object_size(sessions) <= priv->max_sessions_per_user) {
            break;
        }
        /*-------------------------------*
         *  Check max sessions allowed
         *  Drop the old sessions
         *-------------------------------*/
        hgobj prev_channel_gobj = (hgobj)(uintptr_t)kw_get_int(gobj, session, "channel_gobj", 0, KW_REQUIRED);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "Drop session, max sessions reached",
            "user",         "%s", username,
            NULL
        );
        gobj_send_event(prev_channel_gobj, EV_DROP, 0, gobj);
        json_object_del(sessions, k);
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
    if(!kw_has_key(services_roles, dst_service)) {
        /*
         *  No Autorizado
         */
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "Username has not authz in service",
            "user",         "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
        json_decref(services_roles);
        JSON_DECREF(user);
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw)
        return json_pack("{s:i, s:s, s:s, s:s}",
            "result", -1,
            "comment", "Username has not authz in service",
            "dst_service", dst_service,
            "username", username
        );
    }

    /*-------------------------------*
     *      Save session
     *  WARNING "session_state" is from keycloak!!!
     *  And others???
     *-------------------------------*/
    const char *session_id = kw_get_str(
        gobj,
        jwt_payload,
        "sid",
        "",
        KW_REQUIRED
    );
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

    /*------------------------------------*
     *  Subscribe to know close session
     *------------------------------------*/
    gobj_subscribe_event(src, EV_ON_CLOSE, 0, gobj);

    /*--------------------------------*
     *      Autorizado, informa
     *--------------------------------*/
    json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s, s:O, s:O}",
        "result", 0,
        "comment", "JWT User authenticated",
        "username", username,
        "dst_service", dst_service,
        "services_roles", services_roles,
        "jwt_payload", jwt_payload
    );

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

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_AUTH,
        "msg",          "%s", "LOGIN: JWT User authenticated",
        "user",         "%s", username,
        "service",      "%s", dst_service,
        NULL
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

    gobj_send_event(gobj, EV_ADD_USER, json_incref(kw), src);

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
        json_pack("{s:b}",
            "with_metadata", 0
        ),
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

    json_object_set_new(user, "username", json_string(username));
    json_object_set_new(user, "disabled", json_true());
    gobj_send_event(gobj, EV_REJECT_USER, user, src);

    user = gobj_get_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s}", "id", username),
        json_pack("{s:b}",
            "with_metadata", 0
        ),
        gobj
    );

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
    json_t **jwt_payload,
    const char **status
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    BOOL validated = FALSE;
    *jwt_payload = NULL;
    *status = "No OAuth2 Issuer found";

    int idx; json_t *jn_validation;
    json_array_foreach(priv->jn_validations, idx, jn_validation) {
        jwt_checker_t *jwt_checker = (jwt_checker_t *)(uintptr_t)kw_get_int(
            gobj, jn_validation, "jwt_checker", 0, KW_REQUIRED
        );

        *jwt_payload = jwt_checker_verify2(jwt_checker, token);
        if(*jwt_payload) {
            validated = TRUE;
            break;
        }
        // *status = jwt_checker_error_msg(jwt_checker); Don't use jwt messages
    }

    return validated;
}

/***************************************************************************
 * Check if a given username belongs to the "yuneta" group
 * or is exactly the "yuneta" user.
 *
 * Return:
 *      TRUE -> user is "yuneta" or belongs to "yuneta" group
 *      FALSE -> otherwise
 ***************************************************************************/
PRIVATE int is_yuneta_user(const char *username)
{
    if(!username) {
        return FALSE;
    }

    /* Check if user is exactly "yuneta" */
    if(strcmp(username, "yuneta") == 0) {
        return TRUE;
    }

    /* Get passwd entry for user */
    struct passwd *pw = getpwnam(username);
    if(!pw) {
        return FALSE;
    }

    /* Get group entry for "yuneta" */
    struct group *gr = getgrnam("yuneta");
    if(!gr) {
        return FALSE;
    }

    /* Check if user's primary group is yuneta */
    if(pw->pw_gid == gr->gr_gid) {
        return TRUE;
    }

    /* Check supplementary groups */
    gid_t groups[NGROUPS_MAX];
    int ngroups = NGROUPS_MAX;

    if(getgrouplist(username, pw->pw_gid, groups, &ngroups) == -1) {
        /* In case of error, fallback: user likely has more groups than NGROUPS_MAX */
        return FALSE;
    }

    for(int i = 0; i < ngroups; i++) {
        if(groups[i] == gr->gr_gid) {
            return TRUE;
        }
    }

    return FALSE;
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
    json_t *jwt_payload,
    const char *peername
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->master) {
        /*
         *  Crea user en users_accesses
         */
        json_t *access = json_pack("{s:s, s:s, s:I, s:s, s:O}",
            "id", username,
            "ev", "login",
            "tm", (json_int_t)time_in_seconds(),
            "ip", peername,
            "jwt_payload", jwt_payload
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
 *      Web clients (__top_side__)
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
            "msg",          "%s", "open without jwt_payload",
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
    if(priv->tranger) { // Si han pasado a pause es 0
        const char *session_id = kw_get_str(gobj, jwt_payload, "sid", "", KW_REQUIRED);
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
                "msg",          "%s", "LOGOUT",
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

    time_t t;
    time(&t);

    if(empty_string(role)) {
        json_decref(gobj_update_node(
            priv->gobj_treedb,
            "users",
            json_pack("{s:s, s:I, s:b}",
                "id", username,
                "time", (json_int_t)t,
                "disabled", disabled
            ),
            json_pack("{s:b, s:b}",
                "create", 1,
                "autolink", 0
            ),
            src
        ));
    } else {
        json_decref(gobj_update_node(
            priv->gobj_treedb,
            "users",
            json_pack("{s:s, s:s, s:I, s:b}",
                "id", username,
                "roles", role,
                "time", (json_int_t)t,
                "disabled", disabled
            ),
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
    .mt_create = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
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
   If we are here is because gobj_service has not authenticate parser,
   then use this global parser
 ***************************************************************************/
PUBLIC json_t *authenticate_parser(hgobj gobj_service, json_t *kw, hgobj src)
{
    hgobj gobj = gobj_find_service_by_gclass(C_AUTHZ, TRUE);
    if(!gobj) {
        /*
         *  HACK if this function is called is because the authz system is configured in setup.
         *  If the service is not found, deny all.
         */
        KW_DECREF(kw)
        return json_pack("{s:i, s:s}",
            "result", -1,
            "comment", "Authz gclass not found"
        );
    }

    return gobj_authenticate(gobj, kw, src);
}
