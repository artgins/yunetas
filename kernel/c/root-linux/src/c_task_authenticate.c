/***********************************************************************
 *          c_task_authenticate.c
 *          Task_authenticate GClass.
 *
 *          Task to authenticate with OAuth2 against an OIDC IdP.
 *
 *  IdP configuration (priority order — same scheme as c_auth_bff):
 *
 *    1. Explicit endpoints — set both 'token_endpoint' and
 *       'end_session_endpoint' to the full URLs.  Skips discovery.
 *
 *    2. OIDC discovery — set 'issuer' to the IdP issuer URL
 *       (e.g. https://auth.example.com/realms/foo/).  At mt_start the
 *       task chain prepends a discovery GET of
 *       <issuer>/.well-known/openid-configuration; resolved endpoints
 *       are cached in priv before action_get_token runs.
 *
 *    3. Legacy 'auth_url' (DEPRECATED).  Builds
 *       <auth_url>/protocol/openid-connect/{token,logout}.  Emits a
 *       warning at startup; will be removed in a future release.
 *
 *  The 'auth_system' attr no longer routes flow selection (the
 *  SWITCHS body falls through identically for any value).  It is
 *  preserved for back-compat but documented as deprecated.

Example of id_token
-------------------

{
  "exp": 1973085502,
  "iat": 1627485502,
  "auth_time": 0,
  "jti": "868d1822-53ea-41bc-9530-4d39a4443494",
  "iss": "http://localhost:8281/auth/realms/yyyy",
  "aud": "xxxxxx",
  "sub": "277f7140-5dde-4549-ae58-5284e5afb7db",
  "typ": "ID",
  "azp": "xxxxxx",
  "session_state": "8d192831-cfe1-4a25-a42e-4ea71f6f555d",
  "at_hash": "vZbI642n7QbXGHK0MMqsDw",
  "acr": "1",
  "email_verified": TRUE,
  "name": "User Name",
  "preferred_username": "userrrrrr@yyyy.es",
  "locale": "es",
  "given_name": "User Name",
  "family_name": "",
  "email": "userrrrrr@yyyy.es"
}

Example of access_token of keycloak
-----------------------------------

{
  "exp": 1973085502,
  "iat": 1627485502,
  "jti": "e3ebab65-2092-4f59-9498-561c5a72932a",
  "iss": "http://localhost:8281/auth/realms/yyyy",
  "aud": [
    "realm-management",
    "account"
  ],
  "sub": "277f7140-5dde-4549-ae58-5284e5afb7db",
  "typ": "Bearer",
  "azp": "xxxxxx",
  "session_state": "8d192831-cfe1-4a25-a42e-4ea71f6f555d",
  "acr": "1",
  "allowed-origins": [
    "https://yyyy.xxxxxx.com"
  ],
  "realm_access": {
    "roles": [
      "offline_access",
      "uma_authorization",
      "default-roles-yyyy"
    ]
  },
  "resource_access": {
    "realm-management": {
      "roles": [
        "view-realm",
        "view-identity-providers",
        "manage-identity-providers",
        "impersonation",
        "realm-admin",
        "create-client",
        "manage-users",
        "query-realms",
        "view-authorization",
        "query-clients",
        "query-users",
        "manage-events",
        "manage-realm",
        "view-events",
        "view-users",
        "view-clients",
        "manage-authorization",
        "manage-clients",
        "query-groups"
      ]
    },
    "account": {
      "roles": [
        "manage-account",
        "manage-account-links",
        "view-profile"
      ]
    }
  },
  "scope": "openid profile offline_access email",
  "email_verified": TRUE,
  "name": "User Name",
  "preferred_username": "userrrrrr@yyyy.es",
  "locale": "es",
  "given_name": "User Name",
  "family_name": "",
  "email": "userrrrrr@yyyy.es"
}

Example of refresh_token
------------------------

{
  "exp": 1973085502,
  "iat": 1627485502,
  "jti": "05365154-dfd7-4513-a48f-35015bb3746e",
  "iss": "http://localhost:8281/auth/realms/yyyy",
  "aud": "http://localhost:8281/auth/realms/yyyy",
  "sub": "277f7140-5dde-4549-ae58-5284e5afb7db",
  "typ": "Offline",
  "azp": "xxxxxx",
  "session_state": "8d192831-cfe1-4a25-a42e-4ea71f6f555d",
  "scope": "openid profile offline_access email"
}


 *          Copyright (c) 2021 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <command_parser.h>

#include "msg_ievent.h"
#include "c_prot_http_cl.h"
#include "c_task.h"
#include "c_task_authenticate.h"

#include "c_tcp.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level=1: search in bottoms, level=2: search in all childs"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,        cmd_help,       "Command's help"),
SDATA_END()
};


/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag------------default---------description---------- */
SDATA (DTP_BOOLEAN,     "offline_access",   SDF_RD,         0,              "Get offline token"),
SDATA (DTP_JSON,        "crypto",           SDF_RD,         "{}",           "Crypto config"),
SDATA (DTP_STRING,      "issuer",           SDF_RD,         "",             "OIDC issuer URL (e.g. https://auth.example.com/realms/foo/). Triggers discovery via /.well-known/openid-configuration"),
SDATA (DTP_STRING,      "token_endpoint",   SDF_RD,         "",             "Explicit OAuth2 token endpoint URL. Overrides discovery"),
SDATA (DTP_STRING,      "end_session_endpoint", SDF_RD,     "",             "Explicit OIDC end_session endpoint URL. Overrides discovery"),
SDATA (DTP_STRING,      "auth_system",      SDF_RD,         "keycloak",     "DEPRECATED: no longer routes flow selection. Kept for back-compat"),
SDATA (DTP_STRING,      "auth_url",         SDF_RD,         "",             "DEPRECATED: use 'issuer' or explicit endpoints. Legacy Keycloak base URL <auth_url>/protocol/openid-connect/{token,logout}"),
SDATA (DTP_STRING,      "user_id",          SDF_RD,         "",             "OAuth2 User Id (interactive jwt)"),
SDATA (DTP_STRING,      "user_passw",       0,              "",             "OAuth2 User Password (interactive jwt)"),
SDATA (DTP_STRING,      "azp",              SDF_RD,         "",             "OAuth2 Authorized Party  (jwt's azp field - interactive jwt)"),
SDATA (DTP_STRING,      "access_token",     0,              "",             "Access token"),
SDATA (DTP_STRING,      "refresh_token",    0,              "",             "Refresh token"),
SDATA (DTP_POINTER,     "user_data",        0,              0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,              0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,              0,              "subscriber of output-events. Not a child gobj."),
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
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    /*
     *  OIDC endpoints (full URLs).  Resolved in mt_start from one of:
     *    1. Explicit token_endpoint + end_session_endpoint attrs
     *    2. OIDC discovery (<issuer>/.well-known/openid-configuration)
     *    3. Legacy auth_url (Keycloak path scheme — deprecated)
     *  The discovery flow chains action_discover/result_save_discovery
     *  in front of action_get_token in the C_TASK jobs array.
     */
    char    issuer[512];                 /* copy of attr; "" if not configured */
    char    token_url[PATH_MAX];         /* full URL — empty until discovery resolves it */
    char    end_session_url[PATH_MAX];   /* full URL — empty until discovery resolves it */
    BOOL    discovery_done;              /* TRUE once endpoints are usable */

    hgobj gobj_http;
} PRIVATE_DATA;





            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    //PRIVATE_DATA *priv = gobj_priv_data(gobj);

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
    //SET_PRIV(timeout,               gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    //PRIVATE_DATA *priv = gobj_priv_data(gobj);

    //IF_EQ_SET_PRIV(timeout,             gobj_read_integer_attr)
    //END_EQ_SET_PRIV()
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------*
     *  OIDC endpoint resolution priority:
     *    1. Explicit token_endpoint + end_session_endpoint    -> use directly
     *    2. issuer                                            -> discover at startup
     *    3. Legacy auth_url (Keycloak path scheme)            -> deprecated, warn
     *    4. None                                              -> error, refuse to start
     *
     *  http_url is the URL passed to C_PROT_HTTP_CL/C_TCP for the
     *  outbound connection (only host/port matter; the path of each
     *  request is set per-call via the `resource` field).
     *-----------------------------*/
    const char *token_endpoint       = gobj_read_str_attr(gobj, "token_endpoint");
    const char *end_session_endpoint = gobj_read_str_attr(gobj, "end_session_endpoint");
    const char *issuer               = gobj_read_str_attr(gobj, "issuer");
    const char *auth_url             = gobj_read_str_attr(gobj, "auth_url");

    const char *http_url = NULL;

    if(!empty_string(token_endpoint) && !empty_string(end_session_endpoint)) {
        snprintf(priv->token_url, sizeof(priv->token_url), "%s",
            token_endpoint);
        snprintf(priv->end_session_url, sizeof(priv->end_session_url), "%s",
            end_session_endpoint);
        priv->discovery_done = TRUE;
        http_url = token_endpoint;
    } else if(!empty_string(issuer)) {
        snprintf(priv->issuer, sizeof(priv->issuer), "%s", issuer);
        priv->discovery_done = FALSE;
        http_url = issuer;
    } else if(!empty_string(auth_url)) {
        /*
         *  Legacy Keycloak path scheme.  Strip a trailing slash from
         *  auth_url before composing so we don't end up with a double
         *  slash in "<auth_url>//protocol/...".
         */
        size_t len = strlen(auth_url);
        char base[PATH_MAX];
        snprintf(base, sizeof(base), "%s", auth_url);
        if(len > 0 && base[len-1] == '/') {
            base[len-1] = 0;
        }
        snprintf(priv->token_url, sizeof(priv->token_url),
            "%s/protocol/openid-connect/token", base);
        snprintf(priv->end_session_url, sizeof(priv->end_session_url),
            "%s/protocol/openid-connect/logout", base);
        priv->discovery_done = TRUE;
        http_url = auth_url;
        gobj_log_warning(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_PARAMETER,
            "msg",      "%s", "task_authenticate: 'auth_url' is deprecated; use 'issuer' or explicit endpoints",
            "auth_url", "%s", auth_url,
            NULL
        );
    } else {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_PARAMETER,
            "msg",      "%s", "task_authenticate: no IdP configured: set 'issuer' or 'token_endpoint'+'end_session_endpoint'",
            NULL
        );
        return -1;
    }

    /*-----------------------------*
     *      Create http
     *-----------------------------*/
    json_t *jn_crypto = gobj_read_json_attr(gobj, "crypto");

    priv->gobj_http = gobj_create(
        gobj_name(gobj),
        C_PROT_HTTP_CL,
        json_pack("{s:I, s:s}",
            "subscriber", (json_int_t)0,
            "url", http_url
        ),
        gobj
    );
    // HACK Don't subscribe events, will do the tasks
    gobj_unsubscribe_event(priv->gobj_http, NULL, NULL, gobj);

    gobj_set_bottom_gobj(gobj, priv->gobj_http);

    gobj_set_bottom_gobj(
       priv->gobj_http,
       gobj_create_pure_child(
           gobj_name(gobj),
           C_TCP,
           json_pack("{s:s, s:O}", "url", http_url, "crypto", jn_crypto),
           priv->gobj_http
       )
   );

    /*-----------------------------*
     *      Create the task
     *
     *  When discovery is needed, prepend [action_discover,
     *  result_save_discovery] to the existing token/logout chain so
     *  endpoints are resolved on the same connection before the auth
     *  flow runs.
     *-----------------------------*/
    json_t *jobs = json_array();
    if(!priv->discovery_done) {
        json_array_append_new(jobs, json_pack("{s:s, s:s}",
            "exec_action", "action_discover",
            "exec_result", "result_save_discovery"
        ));
    }
    json_array_append_new(jobs, json_pack("{s:s, s:s}",
        "exec_action", "action_get_token",
        "exec_result", "result_get_token"
    ));
    json_array_append_new(jobs, json_pack("{s:s, s:s}",
        "exec_action", "action_logout",
        "exec_result", "result_logout"
    ));

    json_t *kw_task = json_pack(
        "{s:o, s:o, s:o, s:o}",
        "gobj_jobs",    json_integer((json_int_t)(uintptr_t)gobj),
        "gobj_results", json_integer((json_int_t)(uintptr_t)priv->gobj_http),
        "output_data",  json_object(),
        "jobs",         jobs
    );

    hgobj gobj_task = gobj_create(gobj_name(gobj), C_TASK, kw_task, gobj);
    gobj_subscribe_event(gobj_task, EV_END_TASK, 0, gobj);
    gobj_set_volatil(gobj_task, TRUE); // auto-destroy

    /*-----------------------*
     *      Start task
     *-----------------------*/
    gobj_start_tree(priv->gobj_http);
    gobj_start(gobj_task);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    EXEC_AND_RESET(gobj_stop_tree, priv->gobj_http)

    return 0;
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




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int publish_token(
    hgobj gobj,
    int result,
    json_t *kw_)
{
    const char *comment = kw_get_str(gobj, kw_, "comment", "", 0);
    const char *jwt = kw_get_str(gobj, kw_, "jwt", "", 0);

    json_t *kw_on_token = json_pack("{s:i, s:s, s:s}",
        "result", result,
        "comment", comment,
        "jwt", jwt
    );

    gobj_publish_event(gobj, EV_ON_TOKEN, kw_on_token);
    return 0;
}




            /***************************
             *      Jobs
             ***************************/




/***************************************************************************
 *  OIDC discovery: GET <issuer>/.well-known/openid-configuration.
 *  Prepended to the task chain when 'issuer' is configured and no
 *  explicit endpoint override is provided.
 ***************************************************************************/
PRIVATE json_t *action_discover(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src // Source is the GCLASS_TASK
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char ep_schema[32], ep_host[256], ep_port[16], ep_path[1024];
    if(parse_url(gobj,
            priv->issuer,
            ep_schema, sizeof(ep_schema),
            ep_host,   sizeof(ep_host),
            ep_port,   sizeof(ep_port),
            ep_path,   sizeof(ep_path),
            NULL, 0,
            FALSE) < 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_TASK,
            "msg",      "%s", "task_authenticate: invalid issuer URL",
            "issuer",   "%s", priv->issuer,
            NULL
        );
        KW_DECREF(kw)
        STOP_TASK()
    }

    char resource[PATH_MAX];
    build_path(resource, sizeof(resource),
        ep_path, ".well-known", "openid-configuration", NULL);

    json_t *jn_headers = json_pack("{s:s}",
        "Accept", "application/json"
    );
    json_t *query = json_pack("{s:s, s:s, s:s, s:o, s:o}",
        "method",   "GET",
        "resource", resource,
        "query",    "",
        "headers",  jn_headers,
        "data",     json_object()
    );

    gobj_send_event(priv->gobj_http, EV_SEND_MESSAGE, query, gobj);

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *  OIDC discovery: parse the response body, cache token_endpoint and
 *  end_session_endpoint in priv.  Failure publishes EV_ON_TOKEN with
 *  result=-1 so the caller doesn't hang waiting for it.
 ***************************************************************************/
PRIVATE json_t *result_save_discovery(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src // Source is the GCLASS_TASK
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *output_data_ = gobj_read_json_attr(src, "output_data");
    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);
    if(status != 200) {
        json_object_set_new(output_data_, "comment",
            json_sprintf("OIDC discovery failed: status=%d", status)
        );
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_TASK,
            "msg",      "%s", "task_authenticate: OIDC discovery failed",
            "status",   "%d", status,
            "issuer",   "%s", priv->issuer,
            NULL
        );
        publish_token(gobj, -1, output_data_);
        KW_DECREF(kw)
        STOP_TASK()
    }

    json_t *jn_body = kw_get_dict(gobj, kw, "body", NULL, 0);
    const char *token_ep = jn_body ?
        kw_get_str(gobj, jn_body, "token_endpoint", "", 0) : "";
    const char *end_session_ep = jn_body ?
        kw_get_str(gobj, jn_body, "end_session_endpoint", "", 0) : "";

    if(empty_string(token_ep) || empty_string(end_session_ep)) {
        json_object_set_new(output_data_, "comment",
            json_string("OIDC discovery: missing required endpoints in response")
        );
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TASK,
            "msg",                  "%s", "task_authenticate: OIDC discovery missing endpoints",
            "token_endpoint",       "%s", token_ep,
            "end_session_endpoint", "%s", end_session_ep,
            NULL
        );
        publish_token(gobj, -1, output_data_);
        KW_DECREF(kw)
        STOP_TASK()
    }

    snprintf(priv->token_url, sizeof(priv->token_url), "%s", token_ep);
    snprintf(priv->end_session_url, sizeof(priv->end_session_url),
        "%s", end_session_ep);
    priv->discovery_done = TRUE;

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *action_get_token(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src // Source is the GCLASS_TASK
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL offline_access = gobj_read_bool_attr(gobj, "offline_access");
    const char *azp= gobj_read_str_attr(gobj, "azp");
    const char *user_id = gobj_read_str_attr(gobj, "user_id");
    const char *user_passw = gobj_read_str_attr(gobj, "user_passw");
    const char *auth_system = gobj_read_str_attr(gobj, "auth_system");
    SWITCHS(auth_system) {
        CASES("keycloak")
        DEFAULTS
        {
            char resource[PATH_MAX];
            char ep_schema[32], ep_host[256], ep_port[16];
            if(parse_url(gobj,
                    priv->token_url,
                    ep_schema, sizeof(ep_schema),
                    ep_host,   sizeof(ep_host),
                    ep_port,   sizeof(ep_port),
                    resource,  sizeof(resource),
                    NULL, 0,
                    FALSE) < 0) {
                gobj_log_error(gobj, 0,
                    "function",  "%s", __FUNCTION__,
                    "msgset",    "%s", MSGSET_TASK,
                    "msg",       "%s", "task_authenticate: invalid token_endpoint URL",
                    "token_url", "%s", priv->token_url,
                    NULL
                );
                KW_DECREF(kw)
                STOP_TASK()
            }

            json_t *jn_headers = json_pack("{s:s}",
                "Content-Type", "application/x-www-form-urlencoded"
            );

            json_t *jn_data = json_pack("{s:s, s:s, s:s, s:s}",
                "username", user_id,
                "password", user_passw,
                "grant_type", "password",
                "client_id", azp
            );
            if (offline_access) {
                json_object_set_new(jn_data, "scope", json_string( "openid offline_access"));
            }

            json_t *query = json_pack("{s:s, s:s, s:s, s:o, s:o}",
                "method", "POST",
                "resource", resource,
                "query", "",
                "headers", jn_headers,
                "data", jn_data
            );
            gobj_send_event(priv->gobj_http, EV_SEND_MESSAGE, query, gobj);
            break;
        }
    } SWITCHS_END

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *  HACK In this result will publish token.
 *  Actions will continue to do logout if necessary.
 ***************************************************************************/
PRIVATE json_t *result_get_token(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src // Source is the GCLASS_TASK
)
{
    json_t *output_data_ = gobj_read_json_attr(src, "output_data");

    /*------------------------------------*
     *  Http level
     *------------------------------------*/
    int response_status_code = (int)kw_get_int(gobj, kw, "response_status_code", -1, KW_REQUIRED);
    if(response_status_code != 200) {
        json_object_set_new(
            output_data_,
            "comment",
            json_sprintf("Something went wrong, check your user or password: %s, %s",
                llhttp_status_name((llhttp_status_t)response_status_code),
                kw_get_str(gobj, kw, "body`error", "", 0)
            )
        );

        publish_token(gobj, -1, output_data_);

        KW_DECREF(kw)
        STOP_TASK()
    }

    int request_method = (int)kw_get_int(gobj, kw, "request_method", 0, KW_REQUIRED);
    if(request_method) {} // to avoid compilation warning

    json_t *jn_header_ = kw_get_dict(gobj, kw, "headers", 0, KW_REQUIRED);
    if(jn_header_) {} // to avoid compilation warning

    json_t *jn_body_ = kw_get_dict(gobj, kw, "body", 0, KW_REQUIRED);
    if(!jn_body_) {
        json_object_set_new(
            output_data_,
            "comment",
            json_sprintf("http response with no body")
        );
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TASK,
            "msg",          "%s", "Oauth2 response without body",
            NULL
        );
        gobj_trace_json(gobj, kw, "Oauth2 response without body");

        publish_token(gobj, -1, output_data_);

        KW_DECREF(kw)
        STOP_TASK()
    }

    /*-----------------------------------------------*
     *  Response level: keycloak response to login
     *-----------------------------------------------*/
    json_t *jn_response_ = jn_body_;

    const char *access_token = kw_get_str(gobj, jn_response_, "access_token", "", KW_REQUIRED);
    if(access_token) {} // to avoid compilation warning

    const char *refresh_token = kw_get_str(gobj, jn_response_, "refresh_token", "", KW_REQUIRED);
    if(refresh_token) {} // to avoid compilation warning

    const char *id_token = kw_get_str(gobj, jn_response_, "id_token", "", 0); // Only in offline requests
    if(id_token) {} // to avoid compilation warning

    json_int_t expires_in = kw_get_int(gobj, jn_response_, "expires_in", 0, KW_REQUIRED);
    if(expires_in) {} // to avoid compilation warning

    json_int_t refresh_expires_in = kw_get_int(gobj, jn_response_, "refresh_expires_in", 0, KW_REQUIRED);
    if(refresh_expires_in) {} // to avoid compilation warning

    const char *token_type = kw_get_str(gobj, jn_response_, "token_type", "", KW_REQUIRED);
    if(token_type) {} // to avoid compilation warning

    json_int_t not_before_policy = kw_get_int(gobj, jn_response_, "not-before-policy", 0, 0);
    if(not_before_policy) {} // to avoid compilation warning

    const char *session_state = kw_get_str(gobj, jn_response_, "session_state", "", KW_REQUIRED);
    if(session_state) {} // to avoid compilation warning

    const char *scope = kw_get_str(gobj, jn_response_, "scope", "", KW_REQUIRED);
    if(scope) {} // to avoid compilation warning

    if(!empty_string(id_token)) {
        json_object_set_new(output_data_, "comment", json_string("Id Token Ok"));
        json_object_set_new(output_data_, "jwt", json_string(id_token));
    } else if(!empty_string(access_token)) {
        json_object_set_new(output_data_, "comment", json_string("Access Token Ok"));
        json_object_set_new(output_data_, "jwt", json_string(access_token));
    } else {
        json_object_set_new(output_data_, "comment", json_string("No access token in response"));
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TASK,
            "msg",          "%s", "Oauth2 response without id_token or access_token",
            NULL
        );
        gobj_trace_json(gobj, kw, "Oauth2 response without id_token or access_token");

        publish_token(gobj, -1, output_data_);

        KW_DECREF(kw)
        STOP_TASK()
    }

    gobj_write_str_attr(gobj, "access_token", access_token); // Needed for logout
    gobj_write_str_attr(gobj, "refresh_token", refresh_token);

    publish_token(gobj, 0, output_data_);

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *action_logout(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src // Source is the GCLASS_TASK
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *auth_system = gobj_read_str_attr(gobj, "auth_system");
    const char *azp = gobj_read_str_attr(gobj, "azp");
    const char *access_token = gobj_read_str_attr(gobj, "access_token");
    const char *refresh_token = gobj_read_str_attr(gobj, "refresh_token");

    SWITCHS(auth_system) {
        CASES("keycloak")
        DEFAULTS
        {
            char resource[PATH_MAX];
            char ep_schema[32], ep_host[256], ep_port[16];
            if(parse_url(gobj,
                    priv->end_session_url,
                    ep_schema, sizeof(ep_schema),
                    ep_host,   sizeof(ep_host),
                    ep_port,   sizeof(ep_port),
                    resource,  sizeof(resource),
                    NULL, 0,
                    FALSE) < 0) {
                gobj_log_error(gobj, 0,
                    "function",        "%s", __FUNCTION__,
                    "msgset",          "%s", MSGSET_TASK,
                    "msg",             "%s", "task_authenticate: invalid end_session_endpoint URL",
                    "end_session_url", "%s", priv->end_session_url,
                    NULL
                );
                KW_DECREF(kw)
                STOP_TASK()
            }

            char authorization[1024];
            snprintf(authorization, sizeof(authorization), "Bearer %s", access_token);

            json_t *jn_headers = json_pack("{s:s, s:s, s:s}",
                "Content-Type", "application/x-www-form-urlencoded",
                "Authorization", authorization,
                "Connection", "close"
            );

            json_t *jn_data = json_pack("{s:s, s:s}",
                "refresh_token", refresh_token,
                "client_id",  azp
            );

            json_t *query = json_pack("{s:s, s:s, s:s, s:o, s:o}",
                "method", "POST",
                "resource", resource,
                "query", "",
                "headers", jn_headers,
                "data", jn_data
            );
            gobj_send_event(priv->gobj_http, EV_SEND_MESSAGE, query, gobj);
            break;
        }
    } SWITCHS_END


    /*
        url = keycloak_base_url + "/auth/realms/" + keycloak_realm_name + "/protocol/openid-connect/logout"

        cmd = "Logout ==> POST " + url

        headers = CaseInsensitiveDict()
        headers["Authorization"] = "Bearer " + access_token
        headers["Connection"] = "close"
        headers["Content-Type"] = "application/x-www-form-urlencoded"

        data = ""
        form_data = {
            "refresh_token": refresh_token,
            "client_id": client_id
        }
        for k in form_data:
            v = form_data[k]
            if not data:
                data += """%s=%s""" % (k,v)
            else:
                data += """&%s=%s""" % (k,v)

        resp = requests.post(url, headers=headers, data=data, verify=False)
    */

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *result_logout(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src // Source is the GCLASS_TASK
)
{
    /*------------------------------------*
     *  Http level
     *------------------------------------*/
    int response_status_code = (int)kw_get_int(gobj, kw, "response_status_code", -1, KW_REQUIRED);
    if(response_status_code != 204) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TASK,
            "msg",          "%s", "Logout has failed",
            "status_code",  "%d", response_status_code,
            "status",       "%s", llhttp_status_name((llhttp_status_t)response_status_code),
            NULL
        );
        gobj_trace_json(gobj, kw, "Logout has failed");

        KW_DECREF(kw)
        STOP_TASK()
    }

    KW_DECREF(kw)
    CONTINUE_TASK()
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  The token already was published, here only close task.
 ***************************************************************************/
PRIVATE int ac_end_task(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    EXEC_AND_RESET(gobj_stop_tree, priv->gobj_http)

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
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
    .mt_create = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
};

/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {"action_discover",             action_discover,        0},
    {"result_save_discovery",       result_save_discovery,  0},
    {"action_get_token",            action_get_token,       0},
    {"result_get_token",            result_get_token,       0},
    {"action_logout",               action_logout,          0},
    {"result_logout",               result_logout,          0},
    {0, 0, 0}
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TASK_AUTHENTICATE);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_ON_TOKEN);


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
        {EV_END_TASK,           ac_end_task,        0},
        {EV_STOPPED,            ac_stopped,         0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_TOKEN,       EVF_OUTPUT_EVENT},
        {EV_END_TASK,       0},
        {EV_STOPPED,        0},
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
PUBLIC int register_c_task_authenticate(void)
{
    return create_gclass(C_TASK_AUTHENTICATE);
}
