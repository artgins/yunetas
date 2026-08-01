/***********************************************************************
 *          c_idp_keycloak.c
 *          IdpKeycloak GClass.
 *
 *          Provision accounts in a Keycloak identity provider.
 *
 *          WHY THIS IS NOT IN C_AUTHZ.  C_AUTHZ answers one question:
 *          does this user hold this permission, against treedb_authzs.
 *          Creating accounts in an external identity provider is another
 *          responsibility, with other credentials (a confidential admin
 *          client with manage-users), another transport (C_PROT_HTTP_CL
 *          over C_TASK with its own queue) and other failure modes (the
 *          IdP down, a 409, an expired token).  They shared a file by
 *          accident of how it was written, not by design.
 *
 *          ONE GCLASS PER IdP, COMMON VOCABULARY.  The commands are
 *          neutral (`register-idp-user`, not `register-kc-user`) and the
 *          service is instantiated as `idp`.  A second provider enters as
 *          a sibling gclass serving the same commands, chosen in
 *          configuration -- the ytls pattern with OpenSSL / mbedTLS.  No
 *          abstraction layer is invented against a single implementation.
 *
 *          THE SEAM IS AN EVENT.  On a created account this gclass
 *          publishes EV_IDP_USER_CREATED and each plane provisions
 *          itself: C_AUTHZ writes the authorization node, and any other
 *          subscriber writes what it owns.  The dependency points one
 *          way -- the provisioner knows there is an authz plane to
 *          notify, and the authz plane knows of no provider.
 *
 *          The deploy-specific values are configured at run time with
 *          set-kc-config (persistent attrs), never in code or in
 *          committed configuration.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <limits.h>
#include <time.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <command_parser.h>
#include "msg_ievent.h"
#include "c_prot_http_cl.h"
#include "c_task.h"
#include "c_tcp.h"
#include "c_authz.h"
#include "c_idp_keycloak.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
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
    char    role[128];          /* legacy role id, forwarded to the authz plane ("" = none) */
    char    user_id[128];       /* KC user id, from the 201 Location header */
    char    req_service[80];    /* input gate service of the requester */
    char    req_channel[80];    /* requester channel name */
    json_t *kw_request;         /* original request kw (incref), for answer metadata */
    BOOL    authz_ok;           /* the authz plane recorded the user */
    BOOL    answered;           /* answer already sent */
} KC_PENDING;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *kc_config_snapshot(hgobj gobj);
PRIVATE BOOL ensure_kc_client(hgobj gobj);
PRIVATE void process_next_kc(hgobj gobj);
PRIVATE void kc_answer(hgobj gobj, KC_PENDING *p, int result,
    json_t *jn_comment, json_t *jn_data, const char *warning);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_kc_config(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_kc_config(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_register_idp_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

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
SDATAPM (DTP_STRING,    "cmd",          0,      0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,      0,          "level=1: search in bottoms, level=2: search in all childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_STRING,    "authz",        0,      0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,      0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_set_kc_config[] = {
/*-PM----type-----------name----------------------flag----default-description---------- */
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
SDATAPM (DTP_STRING,    "role",         0,              "",     "LEGACY: role id to link in treedb_authzs. Roles belong to the authorization plane; new callers do not send it"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---items---------------json_fn---------------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help, pm_help,            cmd_help,             "Command's help"),
SDATACM (DTP_SCHEMA,    "authzs",           0,      pm_authzs,          cmd_authzs,           "Authorization's help"),

SDATACM2(DTP_SCHEMA,    "set-kc-config",  SDF_AUTHZ_X,  0,  pm_set_kc_config,    cmd_set_kc_config,    "Configure & persist the Keycloak admin connection. Only the params you pass are updated."),
SDATACM2(DTP_SCHEMA,    "view-kc-config", SDF_AUTHZ_X,  0,  0,                   cmd_view_kc_config,   "Show the persisted Keycloak admin config (secret masked)."),
SDATACM2(DTP_SCHEMA,    "register-idp-user", SDF_AUTHZ_X,0, pm_register_idp_user,cmd_register_idp_user,"Register a user in the IdP; the user gets an email to set their password. Publishes EV_IDP_USER_CREATED so each plane records it."),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR---type---------name---------------------flag---------default--description---------- */
SDATA (DTP_STRING,  "kc_base_url",           SDF_PERSIST, "",   "Keycloak base URL for the admin REST API (set via set-kc-config)"),
SDATA (DTP_STRING,  "kc_realm",              SDF_PERSIST, "",   "Keycloak realm where users are created"),
SDATA (DTP_STRING,  "kc_admin_client_id",    SDF_PERSIST, "",   "Confidential admin client_id (client_credentials, role manage-users)"),
SDATA (DTP_STRING,  "kc_admin_client_secret",SDF_PERSIST, "",   "Admin client secret (persisted; never in code or committed config)"),
SDATA (DTP_STRING,  "kc_redirect_uri",       SDF_PERSIST, "",   "redirect_uri for the set-password invite email"),
SDATA (DTP_STRING,  "kc_email_client_id",    SDF_PERSIST, "",   "client_id the invite email links to (the SPA client)"),
SDATA (DTP_JSON,    "kc_crypto",             SDF_RD,      "{\"ssl_use_system_ca\": true, \"ssl_verify_mode\": \"required\"}", "TLS crypto for KC outbound calls. Verifying-by-default against the system CA (public KC). For a private/self-signed KC CA, override with {\"ssl_trusted_certificate\":\"/path/ca.pem\"}. mbedTLS has no system store: set ssl_trusted_certificate there"),
SDATA (DTP_INTEGER, "kc_timeout_ms",         SDF_RD,      "30000","KC outbound round-trip watchdog (ms)"),
SDATA (DTP_POINTER, "user_data",             0,           0,    "user data"),
SDATA (DTP_POINTER, "user_data2",            0,           0,    "more user data"),
SDATA (DTP_POINTER, "subscriber",            0,           0,    "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/

/*---------------------------------------------*
 *      GClass authz levels
 *  The same two permissions the commands had in C_AUTHZ, so a role
 *  granting them keeps working after the move.
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATAAUTHZ (DTP_SCHEMA, "register-idp-user", 0,      0,      0,  "Permission to register a user in the IdP"),
SDATAAUTHZ (DTP_SCHEMA, "configure-kc",      0,      0,      0,  "Permission to read/change the Keycloak admin config"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    /*  One shared outbound HTTP client to Keycloak, serialized through
     *  dl_kc_pending and driven by a volatile multi-job C_TASK. */
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

    /*  The outbound HTTP client is created lazily by ensure_kc_client() on
     *  the first register-idp-user, so it picks up the kc_base_url
     *  configured at run time (set-kc-config, persistent). */
    dl_init(&priv->dl_kc_pending, gobj);
    snprintf(priv->kc_name, sizeof(priv->kc_name), "%s-kc", gobj_name(gobj));

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/

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
}

/***************************************************************************
 *      Framework Method start
 *
 *  Wire the authorization plane to EV_IDP_USER_CREATED.  It is done from
 *  here and not from C_AUTHZ so that no authz code has to know which
 *  provider exists: this gclass knows there is an authz plane to notify,
 *  and the authz plane knows of no provider.  Without an authz service
 *  the event simply has no subscriber, which is why it carries
 *  EVF_NO_WARN_SUBS.
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    hgobj gobj_authz = gobj_find_service_by_gclass(C_AUTHZ, FALSE);
    if(gobj_authz) {
        gobj_subscribe_event(gobj, EV_IDP_USER_CREATED, NULL, gobj_authz);
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
    return gobj_build_authzs_doc(gobj, cmd, kw);
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
            json_sprintf("%s: No kc_* parameter provided", gobj_yuno_role_plus_name()),
            0, 0, kw);
    }
    gobj_save_persistent_attrs(gobj, jn_save);  // owned

    /*  Drop the cached client + token so the new endpoint/credentials take
     *  effect on the next register-idp-user (skip if a round-trip is live).
     *  Stop before destroy: the client can be running (it holds the socket to
     *  the IdP), and destroying it running logs "Destroying a RUNNING gobj". */
    if(priv->gobj_kc && !priv->kc_processing) {
        if(gobj_is_running(priv->gobj_kc)) {
            gobj_stop(priv->gobj_kc);
        }
        gobj_destroy(priv->gobj_kc);
        priv->gobj_kc = 0;
    }
    priv->kc_token[0] = 0;
    priv->kc_token_expiry = 0;

    return msg_iev_build_response(gobj, 0,
        json_sprintf("%s: Keycloak admin config saved", gobj_yuno_role_plus_name()), 0,
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
 *  cmd register-idp-user — create the user in the IdP and let every plane
 *  record it (deferred async answer).
 *
 *  The legacy `role` is validated by asking the authorization plane, which
 *  owns the roles: keeping the check here means an unknown role is refused
 *  BEFORE anything is created in the IdP, the same as before the split.
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
        hgobj gobj_authz = gobj_find_service_by_gclass(C_AUTHZ, FALSE);
        json_t *jn_answer = gobj_authz?
            gobj_local_method(gobj_authz, "has_role",
                json_pack("{s:s}", "role", role), gobj) : NULL;
        BOOL ok = jn_answer? kw_get_bool(gobj, jn_answer, "found", 0, 0) : FALSE;
        JSON_DECREF(jn_answer)
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




                    /***************************
                     *      Local Methods
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

    /*
     *  C_PROT_HTTP_CL is a CHILD gclass: it subscribes its parent to every
     *  event it publishes. This parent wants none of them. The audience is
     *  the C_TASK, which subscribes to gobj_results by itself. Left
     *  subscribed in C_AUTHZ, the close of this outbound socket entered its
     *  ac_on_close and logged out a user that does not exist, while the
     *  answers of Keycloak were lost in "Event NOT DEFINED in state".
     *  Same treatment as the IdP client of C_AUTH_BFF.
     */
    gobj_unsubscribe_event(priv->gobj_kc, NULL, NULL, gobj);

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
 *  Job 2 result: on 201, read user id from Location and publish
 *  EV_IDP_USER_CREATED so every plane records the user; map every other
 *  status to a stable error_code.
 *
 *  gobj_publish_event() returns the sum of the subscriber returns, and an
 *  action returns 0 when it succeeded: a negative sum therefore means a
 *  plane could not record the user, which the final job reports as a
 *  non-fatal warning.
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

        int ret = gobj_publish_event(
            gobj,
            EV_IDP_USER_CREATED,
            json_pack("{s:s, s:s, s:s, s:s, s:s}",
                "username",    p->email,
                "first_name",  p->first_name,
                "last_name",   p->last_name,
                "role",        p->role,
                "idp_user_id", p->user_id
            )
        );
        p->authz_ok = (ret >= 0)? TRUE : FALSE;
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
 *  the IdP, so this is always result 0 — a failed invite or a plane that
 *  could not record the user is reported as a non-fatal `warning`.
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




                    /***************************
                     *      Actions
                     ***************************/




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

    /*
     *  A timed-out round trip leaves the connection poisoned: the late answer
     *  would arrive while the NEXT task owns the client. Drop it here, before
     *  that task starts.
     */
    if(result == -2 && priv->gobj_kc && gobj_is_running(priv->gobj_kc)) {
        gobj_stop(priv->gobj_kc);
    }

    KW_DECREF(kw)
    process_next_kc(gobj);

    /*
     *  Idle: close the connection to Keycloak. C_TCP retries for ever, so a
     *  client left running reconnects once a minute with no request behind
     *  it — hundreds of round trips against the IdP per run of the yuno, all
     *  of them publishing to nobody. process_next_kc() starts it again when
     *  a request arrives, and each request then gets a fresh connection.
     */
    if(!priv->kc_processing && priv->gobj_kc && gobj_is_running(priv->gobj_kc)) {
        gobj_stop(priv->gobj_kc);
    }

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
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_IDP_KEYCLOAK);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

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
 *          Create the GClass
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
        {EV_END_TASK,             ac_end_task,              0},
        {EV_STOPPED,              ac_stopped,               0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        /*  A yuno can provision accounts without an authz plane, so a
         *  missing subscriber is not a bug. */
        {EV_IDP_USER_CREATED,   EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_END_TASK,           0},
        {EV_STOPPED,            0},
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
        command_table,
        0,  // s_user_trace_level
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
PUBLIC int register_c_idp_keycloak(void)
{
    return create_gclass(C_IDP_KEYCLOAK);
}
