/***********************************************************************
 *          c_authz.c
 *          Authz GClass.
 *
 *          Authorization Manager
 *
 *          Copyright (c) 2020 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <unistd.h>
#include <grp.h>
#include <string.h>
#include <limits.h>
#include <jwt.h>

#include <helpers.h>
#include <kwid.h>
#include <timeranger2.h>
#include <tr_msg.h>
#include <tr_treedb.h>
#include "msg_ievent.h"
#include "c_tranger.h"
#include "c_node.h"
#include "c_authz.h"

#include "treedb_schema_authzs.c"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int add_user_login(hgobj gobj, const char *username, json_t *jwt_payload);

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

PRIVATE int create_jwt_validations(hgobj gobj);
PRIVATE int destroy_jwt_validations(hgobj gobj);
PRIVATE int create_validation(hgobj gobj, json_t *jn_pkey);
PRIVATE gbuffer_t *format_to_pem(hgobj gobj, const char *pkey, size_t pkey_len);
PRIVATE BOOL verify_token(hgobj gobj, const char *token, json_t **jwt_payload, const char **status);

/***************************************************************************
 *              Resources
 ***************************************************************************/
/*
 *  OAuth Issuer
 */
static const json_desc_t oauth_iss_desc[] = {
// Name             Type        Defaults    Fillspace
{"iss",             "string",   "",         "60"},  // First item is the pkey
{"description",     "string",   "",         "30"},
{"disabled",        "boolean",  "0",        "8"},
{"algorithm",       "string",   "RS256",    "10"},
{"pkey",            "string",   "",         "20"},
{0}
};


PRIVATE topic_desc_t db_messages_desc[] = {
// Topic Name,          Pkey            System Flag     Tkey        Topic Json Desc
{"users_accesses",      "username",     sf2_string_key, "tm",       0},
{0}
};

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_list_iss(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_add_iss(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_remove_iss(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_enable_iss(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_disable_iss(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_users(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_create_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_enable_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_disable_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_roles(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_user_roles(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_user_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,  "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_add_iss[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "iss",          0,              0,          "Issuer"),
SDATAPM (DTP_STRING, "description",  0,              0,          "Description"),
SDATAPM (DTP_BOOLEAN,   "disabled",     0,              0,          "Disabled"),
SDATAPM (DTP_STRING, "algorithm",    0,              0,          "Algorithm"),
SDATAPM (DTP_STRING, "pkey",         0,              0,          "Public key"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_rm_iss[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "iss",          0,              0,          "Issuer"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING, "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_users[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_JSON,      "filter",       0,              0,          "Filter"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_create_user[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "username",     0,              0,          "Username"),
SDATAPM (DTP_STRING, "role",         0,              0,          "Role, format: roles^ROLE^users"),
SDATAPM (DTP_BOOLEAN,   "disabled",     0,              0,          "Disabled"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_enable_user[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "username",     0,              0,          "Username"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_disable_user[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "username",     0,              0,          "Username"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_roles[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_JSON,      "filter",       0,              0,          "Filter"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_user_roles[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "username",     0,              0,          "Username"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_user_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "username",     0,              0,          "Username"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help, pm_help,        cmd_help,       "Command's help"),

SDATACM (DTP_SCHEMA,    "list-iss",         0,      0,              cmd_list_iss,   "List OAuth2 Issuers"),
SDATACM (DTP_SCHEMA,    "add-iss",          0,      pm_add_iss,     cmd_add_iss,    "Add OAuth2 Issuer"),
SDATACM (DTP_SCHEMA,    "remove-iss",       0,      pm_rm_iss,      cmd_remove_iss, "Remove OAuth2 Issuer"),
SDATACM (DTP_SCHEMA,    "enable-iss",       0,      pm_rm_iss,      cmd_enable_iss, "Enable OAuth2 Issuer"),
SDATACM (DTP_SCHEMA,    "disable-iss",      0,      pm_rm_iss,      cmd_disable_iss,"Disable OAuth2 Issuer"),

SDATACM (DTP_SCHEMA,    "authzs",           0,      pm_authzs,      cmd_authzs,     "Authorization's help"),
SDATACM (DTP_SCHEMA,    "users",            0,      pm_users,       cmd_users,      "List users and their roles"),
SDATACM (DTP_SCHEMA,    "create-user",      0,      pm_create_user, cmd_create_user,"Create or update user (see ROLE format)"),
SDATACM (DTP_SCHEMA,    "enable-user",      0,      pm_enable_user, cmd_enable_user,"Enable user"),
SDATACM (DTP_SCHEMA,    "disable-user",     0,      pm_disable_user,cmd_disable_user,"Disable user"),
SDATACM (DTP_SCHEMA,    "roles",            0,      pm_roles,       cmd_roles,      "List roles"),
SDATACM (DTP_SCHEMA,    "user-roles",       0,      pm_user_roles,  cmd_user_roles, "Get roles of user"),
SDATACM (DTP_SCHEMA,    "user-authzs",      0,      pm_user_authzs, cmd_user_authzs,"Get permissions of user"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag----------------default-----description---------- */
SDATA (DTP_INTEGER,     "max_sessions_per_user",SDF_PERSIST,    "1",          "Max sessions per user"),
SDATA (DTP_STRING,   "jwt_public_key",   SDF_WR|SDF_PERSIST, "",         "JWT public key, for use case: only one iss"),
SDATA (DTP_JSON,        "jwt_public_keys",  SDF_WR|SDF_PERSIST, "[]",       "JWT public keys"),
SDATA (DTP_JSON,        "initial_load",     SDF_RD,             0,          "Initial data for treedb"),
// HACK WARNING 2024-Jul-30, now if tranger_path is set then it's a client (not master)
SDATA (DTP_STRING,   "tranger_path",     SDF_RD,             "",         "Tranger path, internal value (or not)"),
SDATA (DTP_BOOLEAN,     "master",           SDF_RD,             FALSE,      "the master is the only that can write, internal value"),
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

    json_t *users_accesses;      // dict with users opened
    json_t *jn_validations;
} PRIVATE_DATA;

PRIVATE hgclass __gclass__ = 0;




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

    /*
     *  Chequea schema fichador, exit si falla.
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

    /*---------------------------*
     *      OAuth
     *---------------------------*/
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gobj_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    jwt_set_alloc(
        malloc_func,
        realloc_func,
        free_func
    );
    create_jwt_validations(gobj);

    /*---------------------------*
     *  Create Timeranger
     *---------------------------*/
    const char *path = gobj_read_str_attr(gobj, "tranger_path");
    BOOL master = FALSE;
    if(empty_string(path)) {
        char path_[PATH_MAX];
        yuneta_realm_store_dir(
            path_,
            sizeof(path_),
            gobj_yuno_role(),
            gobj_yuno_realm_owner(),
            gobj_yuno_realm_id(),
            "authzs",
            TRUE
        );
        gobj_write_str_attr(gobj, "tranger_path", path_);
        path = gobj_read_str_attr(gobj, "tranger_path");
        master = TRUE;
    }

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
    gobj_write_bool_attr(gobj, "master", master);

    /*----------------------*
     *  Create Treedb
     *----------------------*/
    const char *treedb_name = "treedb_authzs"; // HACK hardcoded service name
    json_t *kw_resource = json_pack("{s:I, s:s, s:o, s:i}",
        "tranger", (json_int_t)(size_t)priv->tranger,
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
    }

    /*
     *  Do copy of heavy used parameters, for quick access.
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

    if(!gobj_is_running(priv->gobj_treedb)) {
        gobj_start(priv->gobj_treedb);
    }

    if(gobj_topic_size(priv->gobj_treedb, "roles")==0 &&
        gobj_topic_size(priv->gobj_treedb, "users")==0 &&
        gobj_topic_size(priv->gobj_treedb, "authorizations")==0
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
                    "EV_TREEDB_UPDATE_NODE",
                    kw_update_node,
                    gobj
                );
            }
        }
    }

    /*---------------------------*
     *  Open topics as messages
     *---------------------------*/
    trmsg_open_topics(
        priv->tranger,
        db_messages_desc
    );

    /*
     *  To open users accesses
     */
    priv->users_accesses = trmsg_open_list(
        priv->tranger,
        "users_accesses",   // topic
        json_pack("{s:i}",  // filter
            "max_key_instances", 1
        )
    );

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->gobj_treedb);
    gobj_stop(priv->gobj_tranger);

    return 0;
}

/***************************************************************************
 *      Framework Method mt_authenticate
    Ejemplo keycloak:  {
            "acr": "1",
            "allowed-origins": [],
            "aud": ["realm-management", "account"],
            "azp": "yunetacontrol",
            "email": "ginsmar@mulesol.es",
            "email_verified": true,
            "exp": 1666336696,
            "family_name": "Martínez",
            "given_name": "Ginés",
            "iat": 1666336576,
            "iss": "https://localhost:8641/auth/realms/mulesol",
            "jti": "96b60323-05c1-4cb1-87e8-8bd68e25a56c",
            "locale": "en",
            "name": "Ginés Martínez",
            "preferred_username": "ginsmar@mulesol.es",
            "realm_access": {},
            "resource_access": {},
            "scope": "profile email",
            "session_state": "aa4fb7ce-d0c7-48a0-ae92-253ef5a600d2",
            "sid": "aa4fb7ce-d0c7-48a0-ae92-253ef5a600d2",
            "sub": "0a1e5c27-80f1-4225-943a-edfbc204972d",
            "typ": "Bearer"
        }
    Ejemplo de jwt dado por google  {
            "aud": "990339570472-k6nqn1tpmitg8pui82bfaun3jrpmiuhs.apps.googleusercontent.com",
            "azp": "990339570472-k6nqn1tpmitg8pui82bfaun3jrpmiuhs.apps.googleusercontent.com",
            "email": "ginsmar@gmail.com",
            "email_verified": true,
            "exp": 1666341427,
            "given_name": "Gins",
            "iat": 1666337827,
            "iss": "https://accounts.google.com",
            "jti": "b2a78ed2911514e30e51fb7b0da3c2032ba3a0aa",
            "name": "Gins",
            "nbf": 1666337527,
            "picture": "https://lh3.googleusercontent.com/a/ALm5wu0soemzAFPT0aSqz_-PyPBX_y9RXuSpRcwStQLRBg=s96-c",
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
    const char *dst_service = kw_get_str(
        kw,
        "__md_iev__`ievent_gate_stack`0`dst_service",
        "",
        KW_REQUIRED
    );
    if(!gobj_find_service(dst_service, FALSE)) {
        KW_DECREF(kw);
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
            KW_DECREF(kw);
            return json_pack("{s:i, s:s}",
                "result", -1,
                "comment", "Ip denied"
            );
        }
        struct passwd *pw = getpwuid(getuid());
        username = pw->pw_name;

        json_t *user = identify_system_user(gobj, &username, TRUE, FALSE);
        if(!user) {
            KW_DECREF(kw);
            return json_pack("{s:i, s:s, s:s}",
                "result", -1,
                "comment", "System user not found or not authorized",
                "username", username
            );
        }
        json_decref(user);

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
                KW_DECREF(kw);
                return json_pack("{s:i, s:s}",
                    "result", -1,
                    "comment", "Without JWT only localhost is allowed"
                );
            }
            comment = "Local Ip allowed";
        } while(0);

        json_t *services_roles = get_user_roles(
            gobj,
            gobj_yuno_realm_id(),
            dst_service,
            username,
            kw
        );

        if(!kw_has_key(services_roles, dst_service)) {
            KW_DECREF(kw);
            return json_pack("{s:i, s:s, s:s, s:s}",
                "result", -1,
                "comment", "Username has not authz in service",
                "dst_service", dst_service,
                "username", username
            );
        }

        /*------------------------------------------------*
         *  HACK guarda username en src (IEvent_srv)
         *------------------------------------------------*/
        gobj_write_str_attr(src, "__username__", username);

        /*
         *  Autorizado
         */
        KW_DECREF(kw);
        return json_pack("{s:i, s:s, s:s, s:s, s:o, s:o}",
            "result", 0,
            "comment", comment,
            "username", username,
            "dst_service", dst_service,
            "services_roles", services_roles,
            "jwt_payload", json_null()
        );
    }

    /*-------------------------------*
     *      HERE with user JWT
     *-------------------------------*/
    json_t *jwt_payload = NULL;
    const char *status;
    if(!verify_token(gobj, jwt, &jwt_payload, &status)) {
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw);
        return json_pack("{s:i, s:s}",
            "result", -1,
            "comment", status
        );
    }

    /*-------------------------------------------------*
     *  Get username and validate against our system
     *-------------------------------------------------*/
    username = kw_get_str(jwt_payload, "email", "", KW_REQUIRED);
    BOOL email_verified = kw_get_bool(jwt_payload, "email_verified", false, KW_REQUIRED);
    if(!email_verified) {
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw);
        return json_pack("{s:i, s:s, s:s}",
            "result", -1,
            "comment", "Email not verified",
            "username", username
        );
    }
    if(!strchr(username, '@')) {
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw);
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
            "EV_AUTHZ_USER_NEW",
            json_pack("{s:s, s:s}",
                "username", username,
                "dst_service", dst_service
            )
        );

        json_t *jn_msg = json_pack("{s:i, s:s, s:s}",
            "result", -1,
            "comment", "User not authorized",
            "username", username
        );
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw);
        return jn_msg;
    }

    BOOL disabled = kw_get_bool(user, "disabled", 0, KW_REQUIRED);
    if(disabled) {
        json_t *jn_msg = json_pack("{s:i, s:s, s:s}",
            "result", -1,
            "comment", "User disabled",
            "username", username
        );
        json_decref(user);
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw);
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
    add_user_login(gobj, username, jwt_payload);

    /*--------------------------------------------*
     *  Get sessions, check max sessions allowed
     *--------------------------------------------*/
    json_t *sessions = kw_get_dict(user, "__sessions", 0, KW_REQUIRED);
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
    json_object_foreach_safe(sessions, n, k, session) {
        if(json_object_size(sessions) <= priv->max_sessions_per_user) {
            break;
        }
        /*-------------------------------*
         *  Check max sessions allowed
         *  Drop the old sessions
         *-------------------------------*/
        hgobj prev_channel_gobj = (hgobj)(size_t)kw_get_int(session, "channel_gobj", 0, KW_REQUIRED);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Drop session, max sessions reached",
            "user",         "%s", username,
            NULL
        );
        gobj_send_event(prev_channel_gobj, "EV_DROP", 0, gobj);
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
        json_decref(services_roles);
        JSON_DECREF(user);
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw);
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
    const char *session_id = kw_get_str(jwt_payload, "session_state", 0, KW_REQUIRED);
    session = json_pack("{s:s, s:I}",
        "id", session_id,
        "channel_gobj", (json_int_t)(size_t)src
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
    gobj_subscribe_event(src, "EV_ON_CLOSE", 0, gobj);

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
        "EV_AUTHZ_USER_LOGIN",
        json_pack("{s:s, s:s, s:o, s:o, s:o, s:o}",
            "username", username,
            "dst_service", dst_service,
            "user", user,
            "session", session,
            "services_roles", services_roles,
            "jwt_payload", jwt_payload
        )
    );

    KW_DECREF(kw);

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
 *  List iss Issuers of OAuth2
 ***************************************************************************/
PRIVATE json_t *cmd_list_iss(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jwt_public_keys = json_deep_copy(gobj_read_json_attr(gobj, "jwt_public_keys"));
    if(!jwt_public_keys) {
        jwt_public_keys = json_array();
    }
    const char *jwt_public_key = gobj_read_str_attr(gobj, "jwt_public_key");
    if(!empty_string(jwt_public_key)) {
        json_array_insert_new(
            jwt_public_keys,
            0,
            json_pack("{s:s, s:s, s:b, s:s, s:s}",
                "iss", "",
                "description", "__default_public_key__",
                "disabled", 0,
                "algorithm", "RS256",
                "pkey", jwt_public_key
            )
        );
    }

    json_t *jn_schema = json_record_to_schema(oauth_iss_desc);

    return msg_iev_build_response(
        gobj,
        0,
        0,
        jn_schema,
        jwt_public_keys,
        kw  // owned
    );
}

/***************************************************************************
 *  Add OAuth2 Issuer
 ***************************************************************************/
PRIVATE json_t *cmd_add_iss(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *iss = kw_get_str(gobj, kw, "iss", "", 0);
    const char *description = kw_get_str(gobj, kw, "description", "", 0);
    BOOL disabled = kw_get_bool(kw, "disabled", 0, KW_WILD_NUMBER);
    const char *algorithm = kw_get_str(gobj, kw, "algorithm", "RS256", 0);
    const char *pkey = kw_get_str(gobj, kw, "pkey", "", 0);

    if(empty_string(iss)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What iss?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(pkey)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What public key?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jwt_public_keys = gobj_read_json_attr(gobj, "jwt_public_keys");
    if(!jwt_public_keys) {
        jwt_public_keys = json_array();
        gobj_write_json_attr(gobj, "jwt_public_keys", jwt_public_keys);
        json_decref(jwt_public_keys);
    }

    /*
     *  Create new record
     */
    json_t *jn_record = create_json_record(oauth_iss_desc);
    json_object_update_new(
        jn_record,
        json_pack("{s:s, s:s, s:b, s:s, s:s}",
            "iss", iss,
            "description", description,
            "disabled", disabled,
            "algorithm", algorithm,
            "pkey", pkey
        )
    );

    /*
     *  Check if the record already exists
     */
    json_t *jn_record_ = kwjr_get( // Return is NOT yours, unless use of KW_EXTRACT
        jwt_public_keys,    // kw, NOT owned
        iss,                // id
        0,                  // new_record, owned
        oauth_iss_desc,     // json_desc
        NULL,               // idx pointer
        0                   // flag
    );
    if(jn_record_) {
        JSON_DECREF(jn_record);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Issuer '%s' already exists", iss),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Add the new record
     */
    jn_record_ = kwjr_get(  // Return is NOT yours, unless use of KW_EXTRACT
        jwt_public_keys,    // kw, NOT owned
        iss,                // id
        jn_record,          // new_record, owned
        oauth_iss_desc,     // json_desc
        NULL,               // idx pointer
        KW_CREATE           // flag
    );

    /*
     *  Save new record in persistent attrs
     */
    gobj_save_persistent_attrs(gobj, json_string("jwt_public_keys"));

    /*
     *  Create new validation
     */
    json_t *jn_validation = create_json_record(oauth_iss_desc);
    json_object_update_new(jn_validation, json_deep_copy(jn_record_));
    json_array_append_new(priv->jn_validations, jn_validation);
    create_validation(gobj, jn_validation);

    return msg_iev_build_response(
        gobj,
        0,
        0,
        json_record_to_schema(oauth_iss_desc),
        json_incref(jn_record_),
        kw  // owned
    );
}

/***************************************************************************
 *  Remove OAuth2 Issuer
 ***************************************************************************/
PRIVATE json_t *cmd_remove_iss(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *iss = kw_get_str(gobj, kw, "iss", "", 0);
    if(empty_string(iss)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What iss?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jwt_public_keys = gobj_read_json_attr(gobj, "jwt_public_keys");

    /*
     *  Check if the record already exists
     */
    json_t *jn_record_ = kwjr_get( // Return is NOT yours, unless use of KW_EXTRACT
        jwt_public_keys,    // kw, NOT owned
        iss,                // id
        0,                  // new_record, owned
        oauth_iss_desc,     // json_desc
        NULL,               // idx pointer
        0                   // flag
    );
    if(!jn_record_) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Issuer '%s' NOT exists", iss),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Remove the record
     */
    jn_record_ = kwjr_get(  // Return is NOT yours, unless use of KW_EXTRACT
        jwt_public_keys,    // kw, NOT owned
        iss,                // id
        0,                  // new_record, owned
        oauth_iss_desc,     // json_desc
        NULL,               // idx pointer
        KW_EXTRACT          // flag
    );

    /*
     *  Save new record in persistent attrs
     */
    gobj_save_persistent_attrs(gobj, json_string("jwt_public_keys"));

    /*
     *  Delete validation
     */
    json_t *jn_validation = kwjr_get(  // Return is NOT yours, unless use of KW_EXTRACT
        priv->jn_validations,    // kw, NOT owned
        iss,                // id
        0,                  // new_record, owned
        oauth_iss_desc,     // json_desc
        NULL,               // idx pointer
        KW_EXTRACT          // flag
    );
    jwt_valid_t *jwt_valid = (jwt_valid_t *)(size_t)kw_get_int(jn_validation, "jwt_valid", 0, KW_REQUIRED);
    jwt_valid_free(jwt_valid);
    JSON_DECREF(jn_validation)

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Issuer '%s' deleted", iss),
        json_record_to_schema(oauth_iss_desc),
        jn_record_,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_disable_iss(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *iss = kw_get_str(gobj, kw, "iss", "", 0);
    if(empty_string(iss)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What iss?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jwt_public_keys = gobj_read_json_attr(gobj, "jwt_public_keys");

    /*
     *  Check if the record already exists
     */
    json_t *jn_record_ = kwjr_get( // Return is NOT yours, unless use of KW_EXTRACT
        jwt_public_keys,    // kw, NOT owned
        iss,                // id
        0,                  // new_record, owned
        oauth_iss_desc,     // json_desc
        NULL,               // idx pointer
        0                   // flag
    );
    if(!jn_record_) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Issuer '%s' NOT exists", iss),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Set disabled in the record
     */
    json_object_set_new(jn_record_, "disabled", json_true());

    /*
     *  Save new record in persistent attrs
     */
    gobj_save_persistent_attrs(gobj, json_string("jwt_public_keys"));

    /*
     *  Delete validation
     */
    json_t *jn_validation = kwjr_get(  // Return is NOT yours, unless use of KW_EXTRACT
        priv->jn_validations,    // kw, NOT owned
        iss,                // id
        0,                  // new_record, owned
        oauth_iss_desc,     // json_desc
        NULL,               // idx pointer
        KW_REQUIRED          // flag
    );
    json_object_set_new(jn_validation, "disabled", json_true());

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Issuer '%s' disabled", iss),
        json_record_to_schema(oauth_iss_desc),
        json_incref(jn_record_),
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_enable_iss(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *iss = kw_get_str(gobj, kw, "iss", "", 0);
    if(empty_string(iss)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What iss?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jwt_public_keys = gobj_read_json_attr(gobj, "jwt_public_keys");

    /*
     *  Check if the record already exists
     */
    json_t *jn_record_ = kwjr_get( // Return is NOT yours, unless use of KW_EXTRACT
        jwt_public_keys,    // kw, NOT owned
        iss,                // id
        0,                  // new_record, owned
        oauth_iss_desc,     // json_desc
        NULL,               // idx pointer
        0                   // flag
    );
    if(!jn_record_) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Issuer '%s' NOT exists", iss),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Set disabled in the record
     */
    json_object_set_new(jn_record_, "disabled", json_false());

    /*
     *  Save new record in persistent attrs
     */
    gobj_save_persistent_attrs(gobj, json_string("jwt_public_keys"));

    /*
     *  Delete validation
     */
    json_t *jn_validation = kwjr_get(  // Return is NOT yours, unless use of KW_EXTRACT
        priv->jn_validations,    // kw, NOT owned
        iss,                // id
        0,                  // new_record, owned
        oauth_iss_desc,     // json_desc
        NULL,               // idx pointer
        KW_REQUIRED          // flag
    );
    json_object_set_new(jn_validation, "disabled", json_false());

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Issuer '%s' enabled", iss),
        json_record_to_schema(oauth_iss_desc),
        json_incref(jn_record_),
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    return gobj_build_authzs_doc(gobj, cmd, kw, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_users(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *jn_filter = kw_get_dict(kw, "filter", 0, KW_EXTRACT);
    char temp[256];
    json_t *jn_users = gobj_list_nodes(
        priv->gobj_treedb,
        "users",
        jn_filter,
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );

    json_t *jn_data = json_array();
    int idx; json_t *jn_user;
    json_array_foreach(jn_users, idx, jn_user) {
        const char *user_id = kw_get_str(jn_user, "id", "", 0);
        json_t *jn_user_roles = kw_get_list(jn_user, "roles", 0, 0);
        json_t *jn_roles_ids = kwid_get_ids(jn_user_roles);
        char *roles_ids = json2uglystr(jn_roles_ids);
        snprintf(temp, sizeof(temp), "%-36s %s", user_id, roles_ids);
        change_char(temp, '"', '\'');
        json_array_append_new(jn_data, json_string(temp));
        JSON_DECREF(jn_roles_ids)
        GBMEM_FREE(roles_ids)
    }

    json_decref(jn_users);

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_data,
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

    gobj_send_event(gobj, "EV_ADD_USER", json_incref(kw), src);

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
            0,
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
        0,
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
    gobj_send_event(gobj, "EV_REJECT_USER", user, src);

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
        0,
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
    json_t *jn_filter = kw_get_dict(kw, "filter", 0, KW_EXTRACT);
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
        0,
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

        json_t *tree_roles = gobj_node_childs(
            priv->gobj_treedb,
            "roles", // topic_name
            role,    // 'id' and pkey2s fields are used to find the node
            "roles",
            json_pack("{s:b}", // filter to childs tree
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

    json_t *jwt_public_keys = json_deep_copy(gobj_read_json_attr(gobj, "jwt_public_keys"));
    const char *jwt_public_key = gobj_read_str_attr(gobj, "jwt_public_key");
    if(!empty_string(jwt_public_key)) {
        json_array_insert_new(
            jwt_public_keys,
            0,
            json_pack("{s:s, s:s, s:b, s:s, s:s}",
                "iss", "",
                "description", "__default_public_key__",
                "disabled", 0,
                "algorithm", "RS256",
                "pkey", jwt_public_key
            )
        );
    }

    priv->jn_validations = json_array();
    int idx; json_t *jn_record;
    json_array_foreach(jwt_public_keys, idx, jn_record) {
        json_t *jn_validation = create_json_record(oauth_iss_desc);
        json_object_update_new(jn_validation, json_deep_copy(jn_record));
        json_array_append_new(priv->jn_validations, jn_validation);
        create_validation(gobj, jn_validation);
    }

    JSON_DECREF(jwt_public_keys)
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
        jwt_valid_t *jwt_valid = (jwt_valid_t *)(size_t)kw_get_int(jn_validation, "jwt_valid", 0, KW_REQUIRED);
        jwt_valid_free(jwt_valid);
    }

    JSON_DECREF(priv->jn_validations)

    return 0;
}

/***************************************************************************
 *  jn_pkey is duplicate json of a entry in jwt_public_keys
 *
 *  jn_pkey: {
 *      iss: str, // Issuer, this claim identifies the entity that issued the JWT
 *      description: str,
 *      algorithm: str, Encryption algorithm
 *      pkey: str, // public key in [raw-base64 | PEM format]
 *  }
 ***************************************************************************/
PRIVATE int create_validation(hgobj gobj, json_t *jn_validation)
{
    const char *iss = kw_get_str(jn_validation, "iss", "", KW_REQUIRED);
    const char *pkey = kw_get_str(jn_validation, "pkey", "", KW_REQUIRED);
    const char *algorithm = kw_get_str(jn_validation, "algorithm", "", KW_REQUIRED);
    int ret = 0;

    /*
     *  Public keys must be in PEM format, convert if not done
     */
    if(strstr(pkey, "-BEGIN PUBLIC KEY-")==NULL) {
        gbuffer_t *gbuf = format_to_pem(gobj, pkey, strlen(pkey));
        const char *p = gbuffer_cur_rd_pointer(gbuf);
        json_object_set_new(jn_validation, "pkey", json_string(p));
        GBUFFER_DECREF(gbuf)
    }

    /*
     *  Convert Encryption algorithm string to enum
     */
    jwt_alg_t alg = jwt_str_alg(algorithm);
    if(alg == JWT_ALG_INVAL) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_CONFIGURATION_ERROR,
            "msg",              "%s", "JWT Algorithm UNKNOWN",
            "algorithm",        "%s", algorithm,
            NULL
        );
        alg = JWT_ALG_RS256;
    }
    json_object_set_new(jn_validation, "alg", json_integer(alg));

    /*
     *  Create validator
     */
    jwt_valid_t *jwt_valid;

    /* Setup validation */
    ret = jwt_valid_new(&jwt_valid, alg);
    if (ret != 0 || jwt_valid == NULL) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_CONFIGURATION_ERROR,
            "msg",              "%s", "jwt_valid_new() FAILED",
            "algorithm",        "%s", algorithm,
            NULL
        );
    } else {
        jwt_valid_set_headers(jwt_valid, 1);
        jwt_valid_set_now(jwt_valid, time(NULL));
        if(!empty_string(iss) && strcmp(iss, "*")!=0) {
            jwt_valid_add_grant(jwt_valid, "iss", iss);
        }
    }

    json_object_set_new(jn_validation, "jwt_valid", json_integer((json_int_t)jwt_valid));

    return ret;
}

/***************************************************************************
 *  Function to convert to PEM format
 ***************************************************************************/
PRIVATE gbuffer_t *format_to_pem(hgobj gobj, const char *pkey, size_t pkey_len)
{
    const char *header = "-----BEGIN PUBLIC KEY-----\n";
    const char *tail = "-----END PUBLIC KEY-----\n";

    size_t l = pkey_len + strlen(header) + strlen(tail) + pkey_len/64 + 1;
    gbuffer_t *gbuf = gbuffer_create(l, l);
    if(!gbuf) {
        // Error already logged
        return NULL;
    }

    gbuffer_append_string(gbuf, header);
    const char *p = pkey;
    size_t lines = pkey_len/64 + ((pkey_len % 64)?1:0);
    for(size_t i=0; i<lines; i++) {
        p += gbuffer_append(gbuf, (void *)p, MIN(64, strlen(p)));
        gbuffer_append_char(gbuf, '\n');
    }
    gbuffer_append_string(gbuf, tail);

    return gbuf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *get_validation_status(unsigned status)
{
    const char *s;
    switch(status) {
        case JWT_VALIDATION_SUCCESS:
            s = "JWT_VALIDATION_SUCCESS";
            break;
        case JWT_VALIDATION_ALG_MISMATCH:
            s = "JWT_VALIDATION_ALG_MISMATCH";
            break;
        case JWT_VALIDATION_EXPIRED:
            s = "JWT_VALIDATION_EXPIRED";
            break;
        case JWT_VALIDATION_TOO_NEW:
            s = "JWT_VALIDATION_TOO_NEW";
            break;
        case JWT_VALIDATION_ISS_MISMATCH:
            s = "JWT_VALIDATION_ISS_MISMATCH";
            break;
        case JWT_VALIDATION_SUB_MISMATCH:
            s = "JWT_VALIDATION_SUB_MISMATCH";
            break;
        case JWT_VALIDATION_AUD_MISMATCH:
            s = "JWT_VALIDATION_AUD_MISMATCH";
            break;
        case JWT_VALIDATION_GRANT_MISSING:
            s = "JWT_VALIDATION_GRANT_MISSING";
            break;
        case JWT_VALIDATION_GRANT_MISMATCH:
            s = "JWT_VALIDATION_GRANT_MISMATCH";
            break;
        default:
        case JWT_VALIDATION_ERROR:
            s = "JWT_VALIDATION_ERROR";
            break;
    }
    return s;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL verify_token(hgobj gobj, const char *token, json_t **jwt_payload, const char **status)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    BOOL validated = FALSE;
    *jwt_payload = NULL;

    jwt_t *jwt;
    int ret;

    int idx; json_t *jn_validation;
    json_array_foreach(priv->jn_validations, idx, jn_validation) {
        BOOL disabled = kw_get_bool(jn_validation, "disabled", 0, KW_REQUIRED);
        if(disabled) {
            *status = "NO OAuth2 Issuer found";
            continue;
        }
        const char *pkey = kw_get_str(jn_validation, "pkey", "", KW_REQUIRED);
        ret = jwt_decode(
            &jwt,
            token,
            (const unsigned char *)pkey,
            (int)strlen(pkey)
        );
        if(ret != 0) {
            *status = "NO OAuth2 Issuer found";
            continue;
        }

        char *s = jwt_get_grants_json(jwt, NULL);
        if(s) {
            *jwt_payload = legalstring2json(s, TRUE);
            jwt_free_str(s);
        }

        jwt_valid_t *jwt_valid = (jwt_valid_t *)(size_t)kw_get_int(gobj, jn_validation, "jwt_valid", 0, KW_REQUIRED);
        jwt_valid_set_now(jwt_valid, time(NULL));

        if(jwt_validate(jwt, jwt_valid)==0) {
            validated = TRUE;
            *status = get_validation_status(jwt_valid_get_status(jwt_valid));
        } else {
            *status = get_validation_status(jwt_valid_get_status(jwt_valid));
            gobj_log_info(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_INFO,
                "msg",              "%s", "jwt invalid",
                "status",           "%s", *status,
                NULL
            );
            gobj_trace_json(gobj, *jwt_payload, "jwt invalid");
        }
        jwt_free(jwt);
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
    BOOL disabled = kw_get_bool(role, "disabled", 0, KW_REQUIRED|KW_WILD_NUMBER);
    if(!disabled) {
        const char *service = kw_get_str(role, "service", "", KW_REQUIRED);
        const char *realm_id = kw_get_str(role, "realm_id", "", KW_REQUIRED);
        if((strcmp(realm_id, dst_realm_id)==0 || strcmp(realm_id, "*")==0)) {
            if(strcmp(service, dst_service)==0 || strcmp(service, "*")==0
            ) {
                json_t *srv_roles = kw_get_list(
                    services_roles,
                    dst_service,
                    json_array(),
                    KW_CREATE
                );
                json_array_append_new(
                    srv_roles,
                    json_string(kw_get_str(role, "id", "", KW_REQUIRED))
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

    json_t *required_services = kw_get_list(kw, "required_services", 0, 0);

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
        if(kw_get_bool(role, "disabled", 0, KW_REQUIRED|KW_WILD_NUMBER)) {
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

        json_t *tree_roles = gobj_node_childs(
            priv->gobj_treedb,
            "roles", // topic_name
            json_incref(role),    // 'id' and pkey2s fields are used to find the node
            "roles",
            json_pack("{s:b}", // filter to childs tree
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
    BOOL disabled = kw_get_bool(role, "disabled", 0, KW_REQUIRED|KW_WILD_NUMBER);
    if(!disabled) {
        const char *service = kw_get_str(role, "service", "", KW_REQUIRED);
        const char *realm_id = kw_get_str(role, "realm_id", "", KW_REQUIRED);
        if((strcmp(realm_id, dst_realm_id)==0 || strcmp(realm_id, "*")==0)) {
            if(strcmp(service, dst_service)==0 || strcmp(service, "*")==0
            ) {
                const char *permission = kw_get_str(role, "permission", "", KW_REQUIRED);
                BOOL deny = kw_get_bool(role, "deny", false, KW_REQUIRED);
                if(!empty_string(permission)) {
                    json_object_set_new(
                        services_roles,
                        permission,
                        deny?json_false():json_true()
                    );
                }

                json_t *permissions = kw_get_list(role, "permissions", 0, KW_REQUIRED);
                int idx; json_t *jn_permission;
                json_array_foreach(permissions, idx, jn_permission) {
                    permission = kw_get_str(jn_permission, "permission", "", KW_REQUIRED);
                    deny = kw_get_bool(jn_permission, "deny", false, KW_REQUIRED);
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

    json_t *required_services = kw_get_list(kw, "required_services", 0, 0);

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
        if(kw_get_bool(role, "disabled", 0, KW_REQUIRED|KW_WILD_NUMBER)) {
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

        json_t *tree_roles = gobj_node_childs(
            priv->gobj_treedb,
            "roles", // topic_name
            json_incref(role),    // 'id' and pkey2s fields are used to find the node
            "roles",
            json_pack("{s:b}", // filter to childs tree
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
PRIVATE int add_user_login(hgobj gobj, const char *username, json_t *jwt_payload)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->master) {
        /*
         *  Crea user en users_accesses
         */
        json_t *user = json_pack("{s:s, s:s, s:I, s:O}",
            "ev", "login",
            "username", username,
            "tm", (json_int_t)time_in_seconds(),
            "jwt_payload", jwt_payload
        );

        trmsg_add_instance(
            priv->tranger,
            "users_accesses",
            user, // owned
            0,
            0
        );
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
        json_t *user = json_pack("{s:s, s:s, s:I}",
            "ev", "logout",
            "username", username,
            "tm", (json_int_t)time_in_seconds()
        );

        trmsg_add_instance(
            priv->tranger,
            "users_accesses",
            user, // owned
            0,
            0
        );
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
        KW_DECREF(kw);
        return 0;
    }

    if(gobj_is_shutdowning()) {
        KW_DECREF(kw);
        return 0;
    }

    /*--------------------------------------------*
     *  Add logout user
     *--------------------------------------------*/
    if(priv->tranger) { // Si han pasado a pause es 0
        const char *session_id = kw_get_str(jwt_payload, "session_state", 0, KW_REQUIRED);
        const char *username = kw_get_str(jwt_payload, "preferred_username", 0, KW_REQUIRED);

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
            json_t *sessions = kw_get_dict(user, "__sessions", 0, KW_REQUIRED);
            json_t *session = kw_get_dict(sessions, session_id, 0, KW_EXTRACT); // Remove session

            add_user_logout(gobj, username);

            gobj_publish_event(
                gobj,
                "EV_AUTHZ_USER_LOGOUT",
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
        }
    }

    gobj_unsubscribe_event(src, "EV_ON_CLOSE", 0, gobj);

    KW_DECREF(kw);
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
    BOOL disabled = kw_get_bool(kw, "disabled", 0, 0);

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

    KW_DECREF(kw);
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
        KW_DECREF(kw);
        return -1;
    }


    if(kw_has_key(kw, "disabled")) {
        BOOL disabled = kw_get_bool(kw, "disabled", 0, 0);
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
    json_t *sessions = kw_get_dict(user, "__sessions", 0, KW_REQUIRED);
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
        hgobj prev_channel_gobj = (hgobj)(size_t)kw_get_int(session, "channel_gobj", 0, KW_REQUIRED);
        gobj_send_event(prev_channel_gobj, "EV_DROP", 0, gobj);
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

    KW_DECREF(kw);
    return ret;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = { // HACK System gclass, not public events
    // top input
    {"EV_ADD_USER",     0,  0,  ""},
    {"EV_REJECT_USER",  0,  0,  ""},
    // bottom input
    {"EV_ON_CLOSE",     0,  0,  ""},
    // internal
    {NULL, 0, 0, ""}
};
PRIVATE const EVENT output_events[] = { // HACK System gclass, not public events
    {"EV_AUTHZ_USER_LOGIN",     0,  0,  ""},
    {"EV_AUTHZ_USER_LOGOUT",    0,  0,  ""},
    {"EV_AUTHZ_USER_NEW",       0,  0,  ""},

    {NULL, 0, 0, ""}
};
PRIVATE const char *state_names[] = {
    "ST_IDLE",
    NULL
};

PRIVATE EV_ACTION ST_IDLE[] = {
    {"EV_ADD_USER",             ac_create_user,            0},
    {"EV_REJECT_USER",          ac_reject_user,         0},
    {"EV_ON_CLOSE",             ac_on_close,            0},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_IDLE,
    NULL
};

PRIVATE FSM fsm = {
    input_events,
    output_events,
    state_names,
    states,
};

/***************************************************************************
 *              GClass
 ***************************************************************************/
/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/*---------------------------------------------*
 *              GClass
 *---------------------------------------------*/
PRIVATE GCLASS _gclass = {
    0,  // base
    GCLASS_AUTHZ_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        mt_destroy,
        mt_start,
        mt_stop,
        0, //mt_play,
        0, //mt_pause,
        mt_writing,
        0, //mt_reading,
        0, //mt_subscription_added,
        0, //mt_subscription_deleted,
        0, //mt_child_added,
        0, //mt_child_removed,
        0, //mt_stats,
        0, //mt_command_parser,
        0, //mt_inject_event,
        0, //mt_create_resource,
        0, //mt_list_resource,
        0, //mt_save_resource,
        0, //mt_delete_resource,
        0, //mt_future21
        0, //mt_future22
        0, //mt_get_resource
        0, //mt_state_changed,
        mt_authenticate,
        0, //mt_list_childs,
        0, //mt_stats_updated,
        0, //mt_disable,
        0, //mt_enable,
        0, //mt_trace_on,
        0, //mt_trace_off,
        0, //mt_gobj_created,
        0, //mt_future33,
        0, //mt_future34,
        0, //mt_publish_event,
        0, //mt_publication_pre_filter,
        0, //mt_publication_filter,
        0, //mt_authz_checker,
        0, //mt_future39,
        0, //mt_create_node,
        0, //mt_update_node,
        0, //mt_delete_node,
        0, //mt_link_nodes,
        0, //mt_future44,
        0, //mt_unlink_nodes,
        0, //mt_topic_jtree,
        0, //mt_get_node,
        0, //mt_list_nodes,
        0, //mt_shoot_snap,
        0, //mt_activate_snap,
        0, //mt_list_snaps,
        0, //mt_treedbs,
        0, //mt_treedb_topics,
        0, //mt_topic_desc,
        0, //mt_topic_links,
        0, //mt_topic_hooks,
        0, //mt_node_parents,
        0, //mt_node_childs,
        0, //mt_list_instances,
        0, //mt_node_tree,
        0, //mt_topic_size,
        0, //mt_future62,
        0, //mt_future63,
        0, //mt_future64
    },
    lmt,
    tattr_desc,
    sizeof(PRIVATE_DATA),
    0,  // acl
    s_user_trace_level,
    command_table,  // command_table
    0,  // gcflag
};

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
    hgobj gobj = gobj_find_gclass_service(GCLASS_AUTHZ_NAME, TRUE);
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
        KW_DECREF(kw);
        return FALSE;
    }

    const char *__username__ = kw_get_str(gobj, kw, "__temp__`__username__", 0, 0);
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
            KW_DECREF(kw);
            return FALSE;
        }
    }

    json_t *jn_authz_desc = gobj_authz(gobj_to_check, authz);
    if(!jn_authz_desc) {
        // Error already logged
        KW_DECREF(kw);
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

    JSON_DECREF(user_authzs);
    JSON_DECREF(jn_authz_desc);
    KW_DECREF(kw);
    return allow;
}

/***************************************************************************
   Authenticate user
   If we are here is because gobj_service has not authenticate parser,
   then use this global parser
 ***************************************************************************/
PUBLIC json_t *authenticate_parser(hgobj gobj_service, json_t *kw, hgobj src)
{
    hgobj gobj = gobj_find_gclass_service(GCLASS_AUTHZ_NAME, TRUE);
    if(!gobj) {
        /*
         *  HACK if this function is called is because the authz system is configured in setup.
         *  If the service is not found, deny all.
         */
        KW_DECREF(kw);
        return json_pack("{s:i, s:s}",
            "result", -1,
            "comment", "Authz gclass not found"
        );
    }

    return gobj_authenticate(gobj, kw, src);
}
