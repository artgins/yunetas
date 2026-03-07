/***********************************************************************
 *          C_AUTH_BFF.C
 *          Auth_bff GClass — Backend For Frontend (BFF) for OAuth2.
 *
 *  SEC-06 — httpOnly cookie token storage:
 *  ========================================
 *  This GClass acts as an HTTP server (not WebSocket) that mediates
 *  between the browser SPA and the Keycloak authorization server.
 *  It exposes three endpoints:
 *
 *    POST /auth/callback
 *      Body: { "code": "…", "code_verifier": "…", "redirect_uri": "…" }
 *      Action: exchange the PKCE authorization code with Keycloak
 *              server-side (grant_type=authorization_code + code_verifier),
 *              write access_token and refresh_token as httpOnly cookies,
 *              return { "success": true, "username": "…", "email": "…",
 *                       "expires_in": N, "refresh_expires_in": N }.
 *
 *    POST /auth/refresh
 *      Reads the "refresh_token" httpOnly cookie from the request.
 *      Action: call Keycloak refresh endpoint, overwrite cookies.
 *      Returns { "success": true, "expires_in": N,
 *                "refresh_expires_in": N }.
 *
 *    POST /auth/logout
 *      Reads the "refresh_token" httpOnly cookie from the request.
 *      Action: call Keycloak logout endpoint, clear cookies (Max-Age=0).
 *      Returns { "success": true }.
 *
 *    OPTIONS *
 *      CORS preflight response.
 *
 *  Architecture:
 *  - Uses C_PROT_HTTP_SR as the lower transport layer.
 *  - For outbound Keycloak calls uses C_PROT_HTTP_CL + C_TASK
 *    (same pattern as C_TASK_AUTHENTICATE).
 *  - One Keycloak request is processed at a time per connection;
 *    concurrent requests are queued in PENDING_AUTH.
 *
 *  Cookie attributes:
 *    HttpOnly    — not accessible from JavaScript
 *    Secure      — HTTPS only
 *    SameSite=Strict — CSRF protection
 *    Path=/      — sent with all requests to this origin
 *    Domain=<configured> — shared between port 1800 (WS) and 1801 (BFF)
 *
 *  Copyright (c) 2025, ArtGins.
 *  All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <time.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <command_parser.h>

#include "msg_ievent.h"
#include "c_prot_http_cl.h"
#include "c_prot_http_sr.h"
#include "c_task.h"
#include "c_tcp.h"
#include "c_auth_bff.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MAX_PENDING_QUEUE   16
#define COOKIE_NAME_AT      "access_token"
#define COOKIE_NAME_RT      "refresh_token"

/*
 *  http-parser method codes (from llhttp / http_parser).
 *  We only need GET (1), POST (3), and OPTIONS (6).
 */
#ifndef HTTP_GET
#define HTTP_GET        1
#define HTTP_POST       3
#define HTTP_OPTIONS    6
#endif

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef enum {
    BFF_CALLBACK,
    BFF_REFRESH,
    BFF_LOGOUT
} bff_action_t;

/*
 *  One queued browser request waiting for its Keycloak response.
 */
typedef struct _PENDING_AUTH {
    hgobj       browser_src;            /* c_prot_http_sr gobj to respond to */
    bff_action_t action;
    char        code[2048];             /* /auth/callback: authorization code */
    char        code_verifier[256];     /* /auth/callback: PKCE verifier */
    char        redirect_uri[1024];     /* /auth/callback: redirect URI */
    char        refresh_token[4096];    /* /auth/refresh + /auth/logout */
    char        client_origin[512];     /* Origin header (for CORS) */
} PENDING_AUTH;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void process_next(hgobj gobj);
PRIVATE void send_json_response(hgobj browser_src, int status_code,
    const char *status_text, json_t *jn_body, const char *extra_headers);
PRIVATE void send_error_response(hgobj browser_src, int status_code,
    const char *status_text, const char *error_msg, const char *extra_headers);
PRIVATE const char *extract_cookie(const char *cookie_header, const char *name,
    char *out, size_t out_size);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t command_table[] = {
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name--------------------flag----default-description*/
SDATA (DTP_STRING,      "keycloak_url",         SDF_RD, "",     "Keycloak base URL, e.g. https://auth.artgins.com/"),
SDATA (DTP_STRING,      "realm",                SDF_RD, "",     "Keycloak realm, e.g. estadodelaire.com"),
SDATA (DTP_STRING,      "client_id",            SDF_RD, "",     "Keycloak client_id (resource)"),
SDATA (DTP_STRING,      "client_secret",        SDF_RD, "",     "Client secret (leave empty for public clients with PKCE)"),
SDATA (DTP_STRING,      "cookie_domain",        SDF_RD, "",     "Cookie Domain attribute (shared hostname without port)"),
SDATA (DTP_STRING,      "allowed_origin",       SDF_RD, "",     "CORS Access-Control-Allow-Origin value"),
SDATA (DTP_STRING,      "allowed_redirect_uri", SDF_RD, "",     "Allowed redirect_uri prefix (e.g. https://treedb.yunetas.com/); rejects callback requests whose redirect_uri does not start with this"),
SDATA (DTP_JSON,        "crypto",               SDF_RD, "{}",   "TLS crypto config for Keycloak outbound calls"),
SDATA (DTP_POINTER,     "user_data",            0,      0,      "user data"),
SDATA (DTP_POINTER,     "user_data2",           0,      0,      "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    /* Request queue */
    PENDING_AUTH    queue[MAX_PENDING_QUEUE];
    int             q_head;             /* next slot to read */
    int             q_tail;             /* next slot to write */
    int             q_count;

    /* Current outbound Keycloak connection */
    BOOL            processing;
    hgobj           gobj_http;          /* C_PROT_HTTP_CL while active */

    /* Parsed Keycloak token endpoint URL parts */
    char schema[32];
    char host[256];
    char port[16];
    char path[1024];
} PRIVATE_DATA;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "user_data");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }
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

    /* Pre-parse Keycloak base URL for re-use in every outbound call */
    char kc_base[1024];
    const char *kc_url  = gobj_read_str_attr(gobj, "keycloak_url");
    const char *realm   = gobj_read_str_attr(gobj, "realm");
    snprintf(kc_base, sizeof(kc_base),
        "%srealms/%s/protocol/openid-connect", kc_url, realm);

    parse_url(gobj,
        kc_base,
        priv->schema, sizeof(priv->schema),
        priv->host,   sizeof(priv->host),
        priv->port,   sizeof(priv->port),
        priv->path,   sizeof(priv->path),
        NULL, 0,
        FALSE
    );

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(priv->gobj_http) {
        gobj_stop_tree(priv->gobj_http);
        priv->gobj_http = NULL;
    }
    return 0;
}




                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *              Helper: HTTP status text
 ***************************************************************************/
PRIVATE const char *status_str(int code)
{
    switch(code) {
        case 200: return "200 OK";
        case 204: return "204 No Content";
        case 400: return "400 Bad Request";
        case 401: return "401 Unauthorized";
        case 403: return "403 Forbidden";
        case 500: return "500 Internal Server Error";
        default:  return "500 Internal Server Error";
    }
}

/***************************************************************************
 *  Build Set-Cookie header string for one token.
 *  Returns a heap-allocated string; caller must GBMEM_FREE it.
 ***************************************************************************/
PRIVATE char *make_set_cookie(const char *name, const char *value,
    int max_age, const char *domain)
{
    char buf[8192];
    int n = snprintf(buf, sizeof(buf),
        "Set-Cookie: %s=%s; Max-Age=%d; Path=/; HttpOnly; Secure; SameSite=Strict",
        name, value, max_age);
    if(!empty_string(domain) && n < (int)sizeof(buf) - 64) {
        snprintf(buf + n, sizeof(buf) - n, "; Domain=%s", domain);
        n = (int)strlen(buf);
    }
    /* append CRLF */
    snprintf(buf + n, sizeof(buf) - n, "\r\n");
    return gbmem_strdup(buf);
}

/***************************************************************************
 *  Build a "clear cookie" Set-Cookie header (Max-Age=0).
 ***************************************************************************/
PRIVATE char *make_clear_cookie(const char *name, const char *domain)
{
    return make_set_cookie(name, "", 0, domain);
}

/***************************************************************************
 *  Extract a named cookie value from a Cookie: header string.
 *  Returns a pointer to `out` or NULL if not found.
 ***************************************************************************/
PRIVATE const char *extract_cookie(const char *cookie_header, const char *name,
    char *out, size_t out_size)
{
    if(empty_string(cookie_header) || empty_string(name) || !out || !out_size) {
        return NULL;
    }
    size_t name_len = strlen(name);
    const char *p = cookie_header;

    while(p && *p) {
        /* skip leading whitespace */
        while(*p == ' ') p++;

        if(strncmp(p, name, name_len) == 0 && p[name_len] == '=') {
            const char *val_start = p + name_len + 1;
            const char *val_end   = strchr(val_start, ';');
            size_t val_len = val_end ? (size_t)(val_end - val_start) : strlen(val_start);
            if(val_len >= out_size) val_len = out_size - 1;
            memcpy(out, val_start, val_len);
            out[val_len] = '\0';
            return out;
        }
        p = strchr(p, ';');
        if(p) p++;
    }
    return NULL;
}

/***************************************************************************
 *  Send an HTTP JSON response to the browser client.
 *
 *  extra_headers: raw header lines ("Set-Cookie: …\r\nSet-Cookie: …\r\n")
 *                 or NULL.
 ***************************************************************************/
PRIVATE void send_json_response(hgobj browser_src, int status_code,
    const char *status_text, json_t *jn_body, const char *extra_headers)
{
    json_t *kw_resp = json_pack("{s:s, s:s, s:O}",
        "code",    status_text,
        "headers", extra_headers ? extra_headers : "",
        "body",    jn_body
    );
    gobj_send_event(browser_src, EV_SEND_MESSAGE, kw_resp, 0);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void send_error_response(hgobj browser_src, int status_code,
    const char *status_text, const char *error_msg, const char *extra_headers)
{
    json_t *jn_body = json_pack("{s:b, s:s}",
        "success", 0,
        "error",   error_msg
    );
    send_json_response(browser_src, status_code, status_text, jn_body, extra_headers);
    JSON_DECREF(jn_body)
}

/***************************************************************************
 *  CORS response headers string (no CRLF terminator after last value —
 *  send_json_response wraps everything in Content-Type + Content-Length).
 ***************************************************************************/
PRIVATE void build_cors_headers(hgobj gobj, const char *origin,
    char *buf, size_t buf_size, BOOL preflight)
{
    const char *allowed_origin = gobj_read_str_attr(gobj, "allowed_origin");
    if(empty_string(allowed_origin)) {
        /*
         *  SEC-06: never reflect an arbitrary Origin or fall back to "*".
         *  If allowed_origin is not configured, deny cross-origin access
         *  by omitting the CORS headers entirely.
         */
        buf[0] = '\0';
        return;
    }
    int n = snprintf(buf, buf_size,
        "Access-Control-Allow-Origin: %s\r\n"
        "Access-Control-Allow-Credentials: true\r\n"
        "Vary: Origin\r\n",
        allowed_origin);
    if(preflight && n < (int)buf_size) {
        snprintf(buf + n, buf_size - n,
            "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Access-Control-Max-Age: 86400\r\n");
    }
}

/***************************************************************************
 *  Enqueue a pending browser request.
 ***************************************************************************/
PRIVATE int enqueue(hgobj gobj, PENDING_AUTH *pa)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(priv->q_count >= MAX_PENDING_QUEUE) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_SYSTEM_ERROR,
            "msg",      "%s", "BFF pending queue full",
            NULL);
        return -1;
    }
    priv->queue[priv->q_tail] = *pa;
    priv->q_tail = (priv->q_tail + 1) % MAX_PENDING_QUEUE;
    priv->q_count++;
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE PENDING_AUTH *dequeue(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(priv->q_count == 0) return NULL;
    PENDING_AUTH *pa = &priv->queue[priv->q_head];
    priv->q_head = (priv->q_head + 1) % MAX_PENDING_QUEUE;
    priv->q_count--;
    return pa;
}

/***************************************************************************
 *  Shared result handler for callback and refresh.
 *  Reads Keycloak token response, sets httpOnly cookies, responds to browser.
 ***************************************************************************/
PRIVATE json_t *result_token_response(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src_task
)
{
    /*
     *  Retrieve and remove the pending request that triggered this call.
     *  (It was passed via output_data of the task.)
     */
    json_t *output_data = gobj_read_json_attr(src_task, "output_data");
    hgobj browser_src   = (hgobj)(uintptr_t)kw_get_int(
        gobj, output_data, "_browser_src", 0, KW_REQUIRED);
    bff_action_t action = (bff_action_t)kw_get_int(
        gobj, output_data, "_action", 0, KW_REQUIRED);

    if(!browser_src) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msg",      "%s", "no browser_src in output_data",
            NULL);
        KW_DECREF(kw)
        STOP_TASK()
    }

    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, KW_REQUIRED);
    const char *origin = kw_get_str(gobj, output_data, "_origin", "", 0);

    char cors_hdrs[1024];
    build_cors_headers(gobj, origin, cors_hdrs, sizeof(cors_hdrs), FALSE);

    if(status != 200) {
        send_error_response(browser_src, 400, status_str(400),
            "Keycloak token exchange failed", cors_hdrs);
        KW_DECREF(kw)
        STOP_TASK()
    }

    json_t *jn_body = kw_get_dict(gobj, kw, "body", NULL, KW_REQUIRED);
    if(!jn_body) {
        send_error_response(browser_src, 500, status_str(500),
            "Empty response from Keycloak", cors_hdrs);
        KW_DECREF(kw)
        STOP_TASK()
    }

    const char *access_token  = kw_get_str(gobj, jn_body, "access_token",  "", 0);
    const char *refresh_token = kw_get_str(gobj, jn_body, "refresh_token", "", 0);
    int64_t     expires_in    = kw_get_int(gobj, jn_body, "expires_in",     300, 0);
    int64_t     ref_exp_in    = kw_get_int(gobj, jn_body, "refresh_expires_in", 1800, 0);

    /* Extract username/email from JWT payload (middle segment) */
    char username[256] = "";
    char email[256]    = "";
    if(!empty_string(access_token)) {
        /* Quick base64url decode of the payload without a full JWT library */
        const char *p1 = strchr(access_token, '.');
        if(p1) {
            const char *p2 = strchr(p1 + 1, '.');
            if(p2) {
                size_t payload_len = (size_t)(p2 - p1 - 1);
                char payload_b64[2048];
                if(payload_len < sizeof(payload_b64) - 4) {
                    memcpy(payload_b64, p1 + 1, payload_len);
                    /* Re-pad to standard base64 */
                    while(payload_len % 4) payload_b64[payload_len++] = '=';
                    payload_b64[payload_len] = '\0';
                    /* Replace URL-safe chars */
                    for(size_t i = 0; i < payload_len; i++) {
                        if(payload_b64[i] == '-') payload_b64[i] = '+';
                        else if(payload_b64[i] == '_') payload_b64[i] = '/';
                    }
                    /* Decode */
                    gbuffer_t *gbuf = gbuffer_base64_to_binary(payload_b64, payload_len);
                    if(gbuf) {
                        char *decoded = gbuffer_cur_rd_pointer(gbuf);
                        json_error_t jerr;
                        json_t *jn_claims = json_loads(decoded, 0, &jerr);
                        if(jn_claims) {
                            const char *e = kw_get_str(gobj, jn_claims, "email", "", 0);
                            const char *u = kw_get_str(gobj, jn_claims,
                                "preferred_username", e, 0);
                            snprintf(username, sizeof(username), "%s", u);
                            snprintf(email,    sizeof(email),    "%s", e);
                            JSON_DECREF(jn_claims)
                        }
                        gbuffer_decref(gbuf);
                    }
                }
            }
        }
    }

    /* Build Set-Cookie headers */
    const char *cookie_domain = gobj_read_str_attr(gobj, "cookie_domain");
    char *at_cookie = make_set_cookie(COOKIE_NAME_AT, access_token,
        (int)expires_in, cookie_domain);
    char *rt_cookie = make_set_cookie(COOKIE_NAME_RT, refresh_token,
        (int)ref_exp_in, cookie_domain);

    /* extra_headers = CORS + two Set-Cookie lines */
    size_t extra_len = strlen(cors_hdrs) + strlen(at_cookie) + strlen(rt_cookie) + 4;
    char *extra = gbmem_malloc(extra_len);
    snprintf(extra, extra_len, "%s%s%s", cors_hdrs, at_cookie, rt_cookie);

    json_t *jn_resp;
    if(action == BFF_CALLBACK) {
        jn_resp = json_pack("{s:b, s:s, s:s, s:I, s:I}",
            "success",          1,
            "username",         username,
            "email",            email,
            "expires_in",       (json_int_t)expires_in,
            "refresh_expires_in", (json_int_t)ref_exp_in
        );
    } else {
        /* BFF_REFRESH */
        jn_resp = json_pack("{s:b, s:I, s:I}",
            "success",          1,
            "expires_in",       (json_int_t)expires_in,
            "refresh_expires_in", (json_int_t)ref_exp_in
        );
    }

    send_json_response(browser_src, 200, status_str(200), jn_resp, extra);

    JSON_DECREF(jn_resp)
    GBMEM_FREE(at_cookie)
    GBMEM_FREE(rt_cookie)
    GBMEM_FREE(extra)

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *  Send the authorization_code or refresh_token request to Keycloak.
 ***************************************************************************/
PRIVATE json_t *action_call_keycloak(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src_task
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *output_data = gobj_read_json_attr(src_task, "output_data");
    bff_action_t action = (bff_action_t)kw_get_int(
        gobj, output_data, "_action", 0, KW_REQUIRED);

    char resource[PATH_MAX];
    snprintf(resource, sizeof(resource), "%s/token", priv->path);

    json_t *jn_headers = json_pack("{s:s}",
        "Content-Type", "application/x-www-form-urlencoded"
    );

    const char *client_id     = gobj_read_str_attr(gobj, "client_id");
    const char *client_secret = gobj_read_str_attr(gobj, "client_secret");
    json_t *jn_data;

    if(action == BFF_CALLBACK) {
        const char *code          = kw_get_str(gobj, output_data, "_code",          "", 0);
        const char *code_verifier = kw_get_str(gobj, output_data, "_code_verifier", "", 0);
        const char *redirect_uri  = kw_get_str(gobj, output_data, "_redirect_uri",  "", 0);

        jn_data = json_pack("{s:s, s:s, s:s, s:s, s:s}",
            "grant_type",    "authorization_code",
            "code",          code,
            "code_verifier", code_verifier,
            "redirect_uri",  redirect_uri,
            "client_id",     client_id
        );
        if(!empty_string(client_secret)) {
            json_object_set_new(jn_data, "client_secret",
                json_string(client_secret));
        }

    } else {
        /* BFF_REFRESH */
        const char *refresh_token = kw_get_str(gobj, output_data, "_refresh_token", "", 0);

        jn_data = json_pack("{s:s, s:s, s:s}",
            "grant_type",    "refresh_token",
            "refresh_token", refresh_token,
            "client_id",     client_id
        );
        if(!empty_string(client_secret)) {
            json_object_set_new(jn_data, "client_secret",
                json_string(client_secret));
        }
    }

    json_t *query = json_pack("{s:s, s:s, s:s, s:o, s:o}",
        "method",   "POST",
        "resource", resource,
        "query",    "",
        "headers",  jn_headers,
        "data",     jn_data
    );
    gobj_send_event(priv->gobj_http, EV_SEND_MESSAGE, query, gobj);

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *  Keycloak logout action.
 ***************************************************************************/
PRIVATE json_t *action_kc_logout(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src_task
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *output_data   = gobj_read_json_attr(src_task, "output_data");
    const char *rt        = kw_get_str(gobj, output_data, "_refresh_token", "", 0);
    const char *client_id = gobj_read_str_attr(gobj, "client_id");
    const char *cs        = gobj_read_str_attr(gobj, "client_secret");

    char resource[PATH_MAX];
    snprintf(resource, sizeof(resource), "%s/logout", priv->path);

    json_t *jn_headers = json_pack("{s:s}", "Content-Type",
        "application/x-www-form-urlencoded");
    json_t *jn_data = json_pack("{s:s, s:s}",
        "refresh_token", rt,
        "client_id",     client_id
    );
    if(!empty_string(cs)) {
        json_object_set_new(jn_data, "client_secret", json_string(cs));
    }

    json_t *query = json_pack("{s:s, s:s, s:s, s:o, s:o}",
        "method",   "POST",
        "resource", resource,
        "query",    "",
        "headers",  jn_headers,
        "data",     jn_data
    );
    gobj_send_event(priv->gobj_http, EV_SEND_MESSAGE, query, gobj);

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *  Result of Keycloak logout.
 ***************************************************************************/
PRIVATE json_t *result_kc_logout(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src_task
)
{
    json_t *output_data = gobj_read_json_attr(src_task, "output_data");
    hgobj browser_src   = (hgobj)(uintptr_t)kw_get_int(
        gobj, output_data, "_browser_src", 0, 0);
    const char *origin  = kw_get_str(gobj, output_data, "_origin", "", 0);

    char cors_hdrs[1024];
    build_cors_headers(gobj, origin, cors_hdrs, sizeof(cors_hdrs), FALSE);

    if(browser_src) {
        const char *cookie_domain = gobj_read_str_attr(gobj, "cookie_domain");
        char *at_clear = make_clear_cookie(COOKIE_NAME_AT, cookie_domain);
        char *rt_clear = make_clear_cookie(COOKIE_NAME_RT, cookie_domain);

        size_t extra_len = strlen(cors_hdrs) + strlen(at_clear) + strlen(rt_clear) + 4;
        char *extra = gbmem_malloc(extra_len);
        snprintf(extra, extra_len, "%s%s%s", cors_hdrs, at_clear, rt_clear);

        json_t *jn_resp = json_pack("{s:b}", "success", 1);
        send_json_response(browser_src, 200, status_str(200), jn_resp, extra);
        JSON_DECREF(jn_resp)
        GBMEM_FREE(at_clear)
        GBMEM_FREE(rt_clear)
        GBMEM_FREE(extra)
    }

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *  Start processing the next pending auth request.
 ***************************************************************************/
PRIVATE void process_next(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->processing || priv->q_count == 0) return;

    PENDING_AUTH *pa = dequeue(gobj);
    if(!pa) return;

    priv->processing = TRUE;

    /* Build Keycloak URL from parsed parts */
    char kc_token_url[1024];
    snprintf(kc_token_url, sizeof(kc_token_url),
        "%s://%s%s%s%s/token",
        priv->schema,
        priv->host,
        empty_string(priv->port) ? "" : ":",
        empty_string(priv->port) ? "" : priv->port,
        priv->path
    );

    json_t *jn_crypto = gobj_read_json_attr(gobj, "crypto");

    /* Create a transient HTTP client for this one Keycloak request */
    priv->gobj_http = gobj_create(
        gobj_name(gobj),
        C_PROT_HTTP_CL,
        json_pack("{s:I, s:s}",
            "subscriber", (json_int_t)0,
            "url",        kc_token_url
        ),
        gobj
    );
    gobj_unsubscribe_event(priv->gobj_http, NULL, NULL, gobj);

    gobj_set_bottom_gobj(gobj, priv->gobj_http);
    gobj_set_bottom_gobj(
        priv->gobj_http,
        gobj_create_pure_child(
            gobj_name(gobj),
            C_TCP,
            json_pack("{s:s, s:O}", "url", kc_token_url, "crypto", jn_crypto),
            priv->gobj_http
        )
    );

    /* Build the task with the appropriate action/result pair */
    json_t *kw_output = json_pack("{s:I, s:i, s:s, s:s, s:s, s:s, s:s}",
        "_browser_src",    (json_int_t)(uintptr_t)pa->browser_src,
        "_action",         (int)pa->action,
        "_code",           pa->code,
        "_code_verifier",  pa->code_verifier,
        "_redirect_uri",   pa->redirect_uri,
        "_refresh_token",  pa->refresh_token,
        "_origin",         pa->client_origin
    );

    json_t *kw_task;
    if(pa->action == BFF_CALLBACK || pa->action == BFF_REFRESH) {
        kw_task = json_pack(
            "{s:o, s:o, s:o, s:["
                "{s:s, s:s},"
                "{s:s, s:s}"
            "]}",
            "gobj_jobs",    json_integer((json_int_t)(uintptr_t)gobj),
            "gobj_results", json_integer((json_int_t)(uintptr_t)priv->gobj_http),
            "output_data",  kw_output,
            "jobs",
                "exec_action", "action_call_keycloak",
                "exec_result", "result_token_response",
                "exec_action", "action_done",
                "exec_result", "result_done"
        );
    } else {
        /* BFF_LOGOUT */
        kw_task = json_pack(
            "{s:o, s:o, s:o, s:["
                "{s:s, s:s},"
                "{s:s, s:s}"
            "]}",
            "gobj_jobs",    json_integer((json_int_t)(uintptr_t)gobj),
            "gobj_results", json_integer((json_int_t)(uintptr_t)priv->gobj_http),
            "output_data",  kw_output,
            "jobs",
                "exec_action", "action_kc_logout",
                "exec_result", "result_kc_logout",
                "exec_action", "action_done",
                "exec_result", "result_done"
        );
    }

    hgobj gobj_task = gobj_create(gobj_name(gobj), C_TASK, kw_task, gobj);
    gobj_subscribe_event(gobj_task, EV_END_TASK, NULL, gobj);
    gobj_set_volatil(gobj_task, TRUE);

    gobj_start_tree(priv->gobj_http);
    gobj_start(gobj_task);
}

/***************************************************************************
 *  Dummy action/result to let C_TASK finish cleanly.
 ***************************************************************************/
PRIVATE json_t *action_done(hgobj gobj, const char *lm, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    STOP_TASK()
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *result_done(hgobj gobj, const char *lm, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    STOP_TASK()
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  New browser HTTP connection opened.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Browser HTTP connection closed (before we could respond, or after).
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Incoming HTTP request from browser.
 *  kw: { url, request_method, headers (json), body (json) }
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    int method = (int)kw_get_int(gobj, kw, "request_method", 0, 0);
    const char *url    = kw_get_str(gobj, kw, "url", "", 0);
    json_t *jn_headers = kw_get_dict(gobj, kw, "headers", NULL, 0);
    json_t *jn_body    = kw_get_dict(gobj, kw, "body",    NULL, 0);

    const char *origin = jn_headers ?
        kw_get_str(gobj, jn_headers, "ORIGIN", "", 0) : "";
    const char *cookie_hdr = jn_headers ?
        kw_get_str(gobj, jn_headers, "COOKIE", "", 0) : "";

    /* Build CORS headers for response */
    char cors_hdrs[1024];
    build_cors_headers(gobj, origin, cors_hdrs, sizeof(cors_hdrs), FALSE);

    /* ---- OPTIONS preflight ---- */
    if(method == HTTP_OPTIONS) {
        char preflight_hdrs[2048];
        build_cors_headers(gobj, origin, preflight_hdrs,
            sizeof(preflight_hdrs), TRUE);
        json_t *jn_empty = json_object();
        send_json_response(src, 204, "204 No Content", jn_empty, preflight_hdrs);
        JSON_DECREF(jn_empty)
        KW_DECREF(kw)
        return 0;
    }

    /* ---- POST only from here ---- */
    if(method != HTTP_POST) {
        send_error_response(src, 405, "405 Method Not Allowed",
            "Only POST is allowed", cors_hdrs);
        KW_DECREF(kw)
        return 0;
    }

    PENDING_AUTH pa;
    memset(&pa, 0, sizeof(pa));
    pa.browser_src = src;
    snprintf(pa.client_origin, sizeof(pa.client_origin), "%s", origin);

    if(strcmp(url, "/auth/callback") == 0) {
        /* POST /auth/callback  { code, code_verifier, redirect_uri } */
        if(!jn_body) {
            send_error_response(src, 400, status_str(400), "Missing JSON body",
                cors_hdrs);
            KW_DECREF(kw)
            return 0;
        }
        const char *code    = kw_get_str(gobj, jn_body, "code",          "", 0);
        const char *cv      = kw_get_str(gobj, jn_body, "code_verifier", "", 0);
        const char *ruri    = kw_get_str(gobj, jn_body, "redirect_uri",  "", 0);

        if(empty_string(code) || empty_string(cv) || empty_string(ruri)) {
            send_error_response(src, 400, status_str(400),
                "code, code_verifier, and redirect_uri are required", cors_hdrs);
            KW_DECREF(kw)
            return 0;
        }

        /*
         *  SEC-06: validate redirect_uri against the configured allowed prefix.
         *  This prevents the BFF from forwarding arbitrary redirect URIs to
         *  Keycloak, which could be exploited in phishing/open-redirect attacks.
         */
        const char *allowed_ruri = gobj_read_str_attr(gobj, "allowed_redirect_uri");
        if(!empty_string(allowed_ruri) &&
                strncmp(ruri, allowed_ruri, strlen(allowed_ruri)) != 0) {
            send_error_response(src, 400, status_str(400),
                "redirect_uri not allowed", cors_hdrs);
            KW_DECREF(kw)
            return 0;
        }

        pa.action = BFF_CALLBACK;
        snprintf(pa.code,          sizeof(pa.code),          "%s", code);
        snprintf(pa.code_verifier, sizeof(pa.code_verifier), "%s", cv);
        snprintf(pa.redirect_uri,  sizeof(pa.redirect_uri),  "%s", ruri);

    } else if(strcmp(url, "/auth/refresh") == 0) {
        /* POST /auth/refresh  (reads httpOnly cookie) */
        char rt[4096];
        if(!extract_cookie(cookie_hdr, COOKIE_NAME_RT, rt, sizeof(rt)) ||
                empty_string(rt)) {
            send_error_response(src, 401, status_str(401),
                "Missing refresh_token cookie", cors_hdrs);
            KW_DECREF(kw)
            return 0;
        }
        pa.action = BFF_REFRESH;
        snprintf(pa.refresh_token, sizeof(pa.refresh_token), "%s", rt);

    } else if(strcmp(url, "/auth/logout") == 0) {
        /* POST /auth/logout  (reads httpOnly cookie) */
        char rt[4096];
        extract_cookie(cookie_hdr, COOKIE_NAME_RT, rt, sizeof(rt));
        pa.action = BFF_LOGOUT;
        snprintf(pa.refresh_token, sizeof(pa.refresh_token), "%s", rt);

    } else {
        send_error_response(src, 404, "404 Not Found", "Unknown endpoint",
            cors_hdrs);
        KW_DECREF(kw)
        return 0;
    }

    if(enqueue(gobj, &pa) < 0) {
        send_error_response(src, 503, "503 Service Unavailable",
            "Server busy, retry in a moment", cors_hdrs);
        KW_DECREF(kw)
        return 0;
    }

    process_next(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Keycloak task completed — clean up, process next queued request.
 ***************************************************************************/
PRIVATE int ac_end_task(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /* Tear down the transient Keycloak HTTP client */
    if(priv->gobj_http) {
        gobj_stop_tree(priv->gobj_http);
        priv->gobj_http = NULL;
    }
    gobj_set_bottom_gobj(gobj, NULL);

    priv->processing = FALSE;

    KW_DECREF(kw)

    /* Immediately process next pending request if any */
    process_next(gobj);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
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

/*---------------------------------------------*
 *          Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {"action_call_keycloak",    action_call_keycloak,   0},
    {"result_token_response",   result_token_response,  0},
    {"action_kc_logout",        action_kc_logout,       0},
    {"result_kc_logout",        result_kc_logout,       0},
    {"action_done",             action_done,            0},
    {"result_done",             result_done,            0},
    {0, 0, 0}
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_AUTH_BFF);

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
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL);
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_ON_OPEN,    ac_on_open,     0},
        {EV_ON_MESSAGE, ac_on_message,  0},
        {EV_ON_CLOSE,   ac_on_close,    0},
        {EV_END_TASK,   ac_end_task,    0},
        {EV_STOPPED,    ac_stopped,     0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_ON_OPEN,    0},
        {EV_ON_MESSAGE, 0},
        {EV_ON_CLOSE,   0},
        {EV_END_TASK,   0},
        {EV_STOPPED,    0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Register GClass
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
        s_user_trace_level,
        0           /* gclass_flag */
    );
    if(!__gclass__) {
        return -1;
    }
    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_auth_bff(void)
{
    return create_gclass(C_AUTH_BFF);
}
