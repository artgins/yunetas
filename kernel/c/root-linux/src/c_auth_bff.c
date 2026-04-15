/***********************************************************************
 *          c_auth_bff.c
 *          Auth_bff GClass — Backend For Frontend (BFF) for OAuth2.
 *
 *  SEC-06 — httpOnly cookie token storage:
 *  ========================================
 *  This GClass acts as an HTTP server (not WebSocket) that mediates
 *  between the browser SPA and the Keycloak authorization server.
 *  It exposes four endpoints:
 *
 *    POST /auth/login
 *      Body: { "username": "…", "password": "…" }
 *      Action: Direct Access Grant (grant_type=password) with Keycloak,
 *              write access_token and refresh_token as httpOnly cookies,
 *              return { "success": true, "username": "…", "email": "…",
 *                       "expires_in": N, "refresh_expires_in": N }.
 *      Note:   Requires "Direct Access Grants Enabled" in the Keycloak
 *              client configuration.
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
 *  Copyright (c) 2026, ArtGins.
 *  All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <strings.h>    /* strcasecmp(), strcasestr() */
#include <stdio.h>
#include <limits.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <command_parser.h>
#include <stats_parser.h>      /* build_stats_response() for mt_stats */

#include "msg_ievent.h"
#include "c_prot_http_cl.h"
#include "c_prot_http_sr.h"
#include "c_task.h"
#include "c_tcp.h"
#include "c_auth_bff.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
/*
 *  Pending request queue sizing.
 *
 *  `pending_queue_size` is a per-instance attr (SDF_RD) so different
 *  channels can be tuned independently — a low-traffic internal BFF
 *  keeps the default, a front-line one exposed to bursty login storms
 *  can raise it without recompiling.
 *
 *  The compile-time ceiling MAX_PENDING_QUEUE_SIZE is a safety cap so
 *  a misconfigured attr can't silently allocate gigabytes: each
 *  PENDING_AUTH entry is ~8 KB, so 1024 slots ≈ 8 MB per channel.
 */
#define DEFAULT_PENDING_QUEUE_SIZE    16
#define MAX_PENDING_QUEUE_SIZE      1024
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
    BFF_LOGIN,
    BFF_REFRESH,
    BFF_LOGOUT
} bff_action_t;

/*
 *  One queued browser request waiting for its Keycloak response.
 */
typedef struct _PENDING_AUTH {
    hgobj       browser_src;            /* c_prot_http_sr gobj to respond to */
    uint64_t    browser_gen;            /* priv->browser_alive_gen captured at enqueue time;
                                           compared against the current priv->browser_alive_gen
                                           when the task's result_* runs to decide whether the
                                           reply belongs to the same browser connection we
                                           enqueued for.  0 would mean "no live connection" and
                                           must never reach result_* — enqueue rejects that case. */
    bff_action_t action;
    char        code[2048];             /* /auth/callback: authorization code */
    char        code_verifier[256];     /* /auth/callback: PKCE verifier */
    char        redirect_uri[1024];     /* /auth/callback: redirect URI */
    char        username[256];          /* /auth/login: username */
    char        password[256];          /* /auth/login: password */
    char        refresh_token[4096];    /* /auth/refresh + /auth/logout */
    char        client_origin[512];     /* Origin header (for CORS) */
    char        client_host[256];       /* Host header (for cookie domain check) */
} PENDING_AUTH;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE BOOL stats_match(const char *stats, const char *name);
PRIVATE const char *action_name(bff_action_t a);
PRIVATE void process_next(hgobj gobj);
PRIVATE void send_json_response(hgobj browser_src, int status_code,
    const char *status_text, json_t *jn_body, const char *extra_headers
);
PRIVATE void send_error_response(hgobj gobj, hgobj browser_src,
    int status_code, const char *status_text,
    const char *error_code, const char *error_msg,
    const char *extra_headers
);
PRIVATE const char *extract_cookie(const char *cookie_header, const char *name,
    char *out, size_t out_size
);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_status(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE const char *a_help[]   = {"h", "?", 0};
PRIVATE const char *a_status[] = {"status", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name------------alias-----------items-----------json_fn-----------description---------- */
SDATACM (DTP_SCHEMA,    "help",         a_help,         pm_help,        cmd_help,         "Command's help"),
SDATACM (DTP_SCHEMA,    "view-status",  a_status,       0,              cmd_view_status,  "Snapshot of this BFF instance: queue, pending tasks and active Keycloak round-trip"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name--------------------flag----default-description*/
SDATA (DTP_STRING,      "keycloak_url",         SDF_RD|SDF_REQUIRED, "", "Keycloak base URL"),
SDATA (DTP_STRING,      "realm",                SDF_RD|SDF_REQUIRED, "", "Keycloak realm"),
SDATA (DTP_STRING,      "client_id",            SDF_RD, "",     "Keycloak client_id (resource)"),
SDATA (DTP_STRING,      "client_secret",        SDF_RD, "",     "Client secret (leave empty for public clients with PKCE)"),
SDATA (DTP_STRING,      "cookie_domain",        SDF_RD, "",     "Cookie Domain attribute (shared hostname without port)"),
SDATA (DTP_STRING,      "allowed_origin",       SDF_RD, "",     "CORS Access-Control-Allow-Origin value"),
SDATA (DTP_STRING,      "allowed_redirect_uri", SDF_RD, "",     "Allowed redirect_uri prefix (e.g. https://treedb.yunetas.com/); rejects callback requests whose redirect_uri does not start with this"),
SDATA (DTP_JSON,        "crypto",               SDF_RD,             "{}",   "TLS crypto config for Keycloak outbound calls"),
SDATA (DTP_INTEGER,     "pending_queue_size",   SDF_RD,             "16",   "Max pending Keycloak requests per channel; clamped to [1, 1024]. Raise for front-line BFFs under burst"),
SDATA (DTP_INTEGER,     "kc_timeout_ms",        SDF_RD,             "30000","Outbound Keycloak watchdog timeout in milliseconds. 0 disables. When a round-trip exceeds this, the BFF sends 504 to the browser and drains the task"),
SDATA (DTP_POINTER,     "user_data",            0,                  0,      "user data"),
SDATA (DTP_POINTER,     "user_data2",           0,                  0,      "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,   /* High-level request flow */
    TRACE_TRAFFIC   = 0x0002,   /* Full HTTP payloads (sensitive fields masked) */
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",    "Trace request flow: connect, request, queue, Keycloak round-trip, response"},
{"traffic",     "Trace full payloads: headers, bodies, Keycloak data (passwords/tokens/codes masked)"},
{0, 0}
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    /* Request queue (ring buffer, size = queue_size) */
    PENDING_AUTH   *queue;
    int             queue_size;         /* active size, clamped from attr */
    int             q_head;             /* next slot to read */
    int             q_tail;             /* next slot to write */
    int             q_count;

    char            idp_name[NAME_MAX]; /* Name for idp gobjs */

    /* Current outbound Identity Provider connection */
    BOOL            processing;
    hgobj           gobj_idprovider;    /* C_PROT_HTTP_CL */
    hgobj           gobj_task;          /* C_TASK */

    hgobj           active_browser_src; /* browser_src of the in-flight task, cleared in ac_end_task */
    uint64_t        active_task_gen;    /* browser generation captured when the in-flight task was dispatched;
                                           0 when no task is active.  ac_kc_watchdog checks this against
                                           priv->browser_alive_gen before sending 504 — if they differ the
                                           connection the task was serving is gone (closed or replaced) and
                                           the 504 must be dropped instead of delivered to an unrelated browser. */

    /*
     *  Browser identity generation counter.
     *
     *  Every ac_on_open bumps `browser_gen_counter` and stores the new
     *  value in `browser_alive_gen` — a monotonically increasing token
     *  that uniquely names the connection currently plugged into this
     *  BFF instance.  ac_on_close sets `browser_alive_gen` back to 0
     *  (sentinel: "no live connection").
     *
     *  Every PENDING_AUTH captures the `browser_alive_gen` that was
     *  current when it was enqueued (`pa->browser_gen`), and that gen
     *  is forwarded through the c_task's output_data as `_browser_gen`.
     *  When result_token_response / result_kc_logout eventually fire
     *  they compare the task's captured gen against the current
     *  `browser_alive_gen`.  Three outcomes:
     *
     *    task_gen == browser_alive_gen && browser_alive_gen != 0
     *        → same connection is still up, forward the reply
     *    browser_alive_gen == 0
     *        → connection closed, drop the reply (responses_dropped++)
     *    task_gen != browser_alive_gen (both non-zero)
     *        → a different connection is now attached to this BFF
     *          instance (persistent_channels=true + rapid reconnect);
     *          the reply belongs to an old user and delivering it to
     *          the new browser would leak that user's token.  Drop.
     *
     *  Replaces the old single-BOOL `browser_alive` flag, which only
     *  covered the "closed" case and was unsafe under persistent
     *  channels with rapid reconnect — see test12_stale_reply in the
     *  test suite for the race scenario.
     */
    uint64_t        browser_gen_counter;
    uint64_t        browser_alive_gen;

    /* Parsed Keycloak token endpoint URL parts */
    char schema[32];
    char host[256];
    char port[16];
    char path[1024];

    /*
     *  Statistics — plain counters in PRIVATE_DATA, NOT SDF_*STATS
     *  attributes, so the hot path stays a single ++ instruction with
     *  no JSON cost.  Exposed via mt_stats() below; mt_stats also
     *  honours the standard "__reset__" stat name to zero them.
     */
    uint64_t        st_requests_total;  /* requests accepted (any endpoint) */
    uint64_t        st_q_max_seen;      /* high-water mark of q_count */
    uint64_t        st_q_full_drops;    /* requests rejected because q full */
    uint64_t        st_kc_calls;        /* Keycloak round-trips started */
    uint64_t        st_kc_ok;           /* Keycloak round-trips with status 200 */
    uint64_t        st_kc_errors;       /* Keycloak round-trips with non-200 */
    uint64_t        st_bff_errors;      /* 4xx/5xx returned to the browser */
    uint64_t        st_responses_dropped; /* KC replies dropped because browser closed */
    uint64_t        st_kc_timeouts;     /* outbound watchdog fired */
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
    // TODO global change keycloak -> idp (Identity Provider)

    /*
     *  Allocate the pending-request ring from the configured size, clamped
     *  to a sane range.  Using gbmem_malloc (which is calloc-backed) gives
     *  us zero-initialised PENDING_AUTH entries for free.
     */
    int requested = (int)gobj_read_integer_attr(gobj, "pending_queue_size");
    if(requested < 1) {
        requested = DEFAULT_PENDING_QUEUE_SIZE;
    } else if(requested > MAX_PENDING_QUEUE_SIZE) {
        gobj_log_warning(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PARAMETER_ERROR,
            "msg",              "%s", "pending_queue_size clamped to MAX_PENDING_QUEUE_SIZE",
            "requested",        "%d", requested,
            "max",              "%d", MAX_PENDING_QUEUE_SIZE,
            NULL
        );
        requested = MAX_PENDING_QUEUE_SIZE;
    }
    priv->queue_size = requested;
    priv->queue = GBMEM_MALLOC(sizeof(PENDING_AUTH) * (size_t)priv->queue_size);
    if(!priv->queue) {
        /* Error already logged by gbmem_malloc */
        priv->queue_size = 0;
    }

    /*
     *  Name for idp gobjs
     */
    snprintf(priv->idp_name, sizeof(priv->idp_name), "%s-idp", gobj_name(gobj));

    /* Pre-parse IdProvider base URL for re-use in every outbound call */
    char kc_base[PATH_MAX];
    const char *kc_url  = gobj_read_str_attr(gobj, "keycloak_url");
    const char *realm   = gobj_read_str_attr(gobj, "realm");

    build_path(kc_base, sizeof(kc_base),
        kc_url,
        "realms",
        realm,
        "protocol",
        "openid-connect",
        NULL
    );

    if(parse_url(gobj,
        kc_base,
        priv->schema, sizeof(priv->schema),
        priv->host,   sizeof(priv->host),
        priv->port,   sizeof(priv->port),
        priv->path,   sizeof(priv->path),   // HACK important
        NULL, 0,
        FALSE
    )<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_SYSTEM_ERROR,
            "msg",      "%s", "keycloak url parse failed",
            "url",      "%s", kc_base,
            NULL
        );
    } else {
        json_t *jn_crypto = gobj_read_json_attr(gobj, "crypto");

        priv->gobj_idprovider = gobj_create(
            priv->idp_name,
            C_PROT_HTTP_CL,
            json_pack("{s:s}",
                "url", kc_url
            ),
            gobj
        );
        gobj_unsubscribe_event(priv->gobj_idprovider, NULL, NULL, gobj);

        gobj_set_bottom_gobj(
            priv->gobj_idprovider,
            gobj_create_pure_child(
                priv->idp_name,
                C_TCP,
                json_pack("{s:s, s:O, s:i}",
                    "url", kc_url,
                    "crypto", jn_crypto,
                    /*
                     *  Aggressive reconnect cadence (100 ms vs the 2 s default).
                     */
                    "timeout_between_connections", 100
                ),
                priv->gobj_idprovider
            )
        );
    }

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
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBMEM_FREE(priv->queue)
    priv->queue_size = 0;
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_task && gobj_is_running(priv->gobj_task)) {
        gobj_stop(priv->gobj_task);
    }
    priv->gobj_task = NULL;

    if(priv->gobj_idprovider && gobj_is_running(priv->gobj_idprovider)) {
        gobj_stop(priv->gobj_idprovider);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stats
 *
 *  Statistics live as plain uint64_t fields in PRIVATE_DATA so the hot
 *  path stays a single ++ instruction (no JSON cost per increment).
 *  This is the override hook called by gobj_stats() — it builds the
 *  stats dict on demand from priv fields and honours the standard
 *  "__reset__" stat name to zero the resettable counters (mirroring
 *  what the default stats_parser.c does for SDF_RSTATS attributes).
 *
 *  `stats` semantics, matching stats_parser.c:
 *    - NULL or ""        → return all stats
 *    - "__reset__"       → reset resettable counters then return all
 *    - "<name|prefix>"   → stats_match() two-stage case-insensitive
 *                          filter: full name OR underscore-prefix of
 *                          the stat's own name appears in <stats>.
 ***************************************************************************/
PRIVATE json_t *mt_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(stats && strcmp(stats, "__reset__") == 0) {
        /*
         *  Reset resettable counters.  q_count is a live gauge mirroring
         *  the current ring depth, not a counter — leave it alone.
         *  q_max_seen restarts from the current depth so the new
         *  high-water mark is anchored to "now".
         */
        priv->st_requests_total = 0;
        priv->st_q_max_seen     = priv->q_count;
        priv->st_q_full_drops       = 0;
        priv->st_kc_calls           = 0;
        priv->st_kc_ok              = 0;
        priv->st_kc_errors          = 0;
        priv->st_bff_errors         = 0;
        priv->st_responses_dropped  = 0;
        priv->st_kc_timeouts        = 0;
        stats = "";  /* fall through and return the (now zeroed) snapshot */
    }

    /*
     *  Build a flat dict of every counter.  Filter by name/prefix if
     *  the caller provided one (same convention as stats_parser.c).
     */
    json_t *jn_data = json_object();

#define STAT_INT(name_, value_) do { \
    if(stats_match(stats, name_)) { \
        json_object_set_new(jn_data, name_, json_integer((json_int_t)(value_))); \
    } \
} while(0)

    STAT_INT("requests_total",      priv->st_requests_total);
    STAT_INT("q_count",             priv->q_count);          /* live gauge */
    STAT_INT("q_max_seen",          priv->st_q_max_seen);
    STAT_INT("q_full_drops",        priv->st_q_full_drops);
    STAT_INT("kc_calls",            priv->st_kc_calls);
    STAT_INT("kc_ok",               priv->st_kc_ok);
    STAT_INT("kc_errors",           priv->st_kc_errors);
    STAT_INT("bff_errors",          priv->st_bff_errors);
    STAT_INT("responses_dropped",   priv->st_responses_dropped);
    STAT_INT("kc_timeouts",         priv->st_kc_timeouts);

#undef STAT_INT

    KW_DECREF(kw)
    return build_stats_response(
        gobj,
        0,          /* result */
        0,          /* jn_comment */
        0,          /* jn_schema */
        jn_data     /* jn_data, owned */
    );
}




                    /***************************
                     *      Commands
                     ***************************/




/***************************************************************************
 *  cmd help
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
 *  cmd view-status — snapshot of this C_AUTH_BFF instance.
 *
 *  Returned data shape:
 *  {
 *      "name":            "bff-3",
 *      "full_name":       "...",
 *      "processing":      true,
 *      "has_active_http": true,
 *      "active_http":     "C_PROT_HTTP_CL^bff-3",
 *      "q_count":         2,
 *      "q_head":          5,
 *      "q_tail":          7,
 *      "q_max":           16,
 *      "queue": [
 *          {"action": "login",    "browser_src": "..."},
 *          {"action": "callback", "browser_src": "..."}
 *      ]
 *  }
 *
 *  C_AUTH_BFF_YUNO's view-bff-status command aggregates this across
 *  every per-channel instance via gobj_command(child, "view-status", ...).
 ***************************************************************************/
PRIVATE json_t *cmd_view_status(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_queue = json_array();
    /*
     *  Walk the ring from q_head forward, q_count entries.
     */
    for(int i = 0; i < priv->q_count; i++) {
        int idx = (priv->q_head + i) % priv->queue_size;
        const PENDING_AUTH *pa = &priv->queue[idx];
        json_t *jn_entry = json_pack("{s:s, s:s}",
            "action",       action_name(pa->action),
            "browser_src",  pa->browser_src ?
                gobj_short_name(pa->browser_src) : ""
        );
        json_array_append_new(jn_queue, jn_entry);
    }

    json_t *jn_data = json_pack("{s:s, s:s, s:b, s:b, s:s, s:i, s:i, s:i, s:i, s:o}",
        "name",             gobj_name(gobj),
        "full_name",        gobj_short_name(gobj),
        "processing",       priv->processing ? 1 : 0,
        "has_active_http",  priv->gobj_idprovider ? 1 : 0,
        "active_http",      priv->gobj_idprovider ? gobj_short_name(priv->gobj_idprovider) : "",
        "q_count",          priv->q_count,
        "q_head",           priv->q_head,
        "q_tail",           priv->q_tail,
        "q_max",            priv->queue_size,
        "queue",            jn_queue
    );

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_data,
        kw  // owned
    );
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Filter predicate matching stats_parser.c::_build_stats().
 *
 *  Two-stage match, mirroring the default parser so an mt_stats override
 *  behaves identically from the caller's point of view:
 *
 *    1. Exact/substring — the stat's full `name` appears inside `stats`.
 *    2. Prefix fallback — extract `name`'s own prefix up to the first '_'
 *       and look for that inside `stats`.  This is what lets a caller
 *       pass "kc_" (or "kc") and receive every `kc_*` counter.
 *
 *  Both lookups are case-insensitive so ad-hoc filters ("KC", "bff")
 *  work the same as the canonical lowercase names.
 ***************************************************************************/
PRIVATE BOOL stats_match(const char *stats, const char *name)
{
    if(empty_string(stats)) {
        return TRUE;
    }
    if(strcasestr(stats, name) != NULL) {
        return TRUE;
    }
    const char *p = strchr(name, '_');
    if(p) {
        char prefix[32];
        size_t ln = (size_t)(p - name);
        if(ln >= sizeof(prefix)) {
            ln = sizeof(prefix) - 1;
        }
        memcpy(prefix, name, ln);
        prefix[ln] = '\0';
        if(strcasestr(stats, prefix) != NULL) {
            return TRUE;
        }
    }
    return FALSE;
}

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
        case 503: return "503 Service Unavailable";
        case 504: return "504 Gateway Timeout";
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
 *  Return the effective cookie domain for the current request.
 *
 *  If `cookie_domain` is configured but the request's Host header hostname
 *  doesn't match it, the browser would reject the cookie ("invalid domain").
 *  This happens during local development when the BFF is configured with a
 *  production domain (e.g. "treedb.yunetas.com") but accessed via localhost.
 *
 *  In that case, return "" (empty) so make_set_cookie() omits the Domain
 *  attribute entirely, letting the cookie bind to the exact request host.
 ***************************************************************************/
PRIVATE const char *effective_cookie_domain(
    hgobj gobj,
    const char *cookie_domain,
    const char *request_host)
{
    if(empty_string(cookie_domain)) {
        return "";  /* not configured — no Domain attribute */
    }
    if(empty_string(request_host)) {
        return cookie_domain;  /* no Host header — keep configured value */
    }

    /* Strip port from request host (e.g. "localhost:5173" → "localhost") */
    char hostname[256];
    snprintf(hostname, sizeof(hostname), "%s", request_host);
    char *colon = strchr(hostname, ':');
    if(colon) *colon = '\0';

    /*
     *  Check: does the request hostname match the configured cookie_domain?
     *  Accept exact match or subdomain match (host ends with "."+domain).
     */
    size_t host_len = strlen(hostname);
    size_t domain_len = strlen(cookie_domain);

    if(host_len == domain_len && strcmp(hostname, cookie_domain) == 0) {
        return cookie_domain;  /* exact match */
    }
    if(host_len > domain_len + 1 &&
            hostname[host_len - domain_len - 1] == '.' &&
            strcmp(hostname + host_len - domain_len, cookie_domain) == 0) {
        return cookie_domain;  /* subdomain match */
    }

    /* Mismatch — omit Domain so the cookie binds to the exact host */
    gobj_log_warning(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msg",              "%s", "cookie_domain does not match request Host, "
                                  "omitting Domain attribute",
        "cookie_domain",    "%s", cookie_domain,
        "request_host",     "%s", request_host,
        NULL
    );
    return "";
}

/***************************************************************************
 *  Trace helpers
 ***************************************************************************/
PRIVATE const char *action_name(bff_action_t a)
{
    switch(a) {
        case BFF_CALLBACK: return "callback";
        case BFF_LOGIN:    return "login";
        case BFF_REFRESH:  return "refresh";
        case BFF_LOGOUT:   return "logout";
        default:           return "?";
    }
}

/*
 *  Return a deep copy of `jn` with sensitive string fields replaced
 *  by "<N chars>".  Used by TRACE_TRAFFIC so payloads can be inspected
 *  in production logs without leaking secrets.
 *  Caller owns the returned object.
 *
 *  Keys are matched case-insensitively against the object's real keys
 *  so HTTP headers (e.g. "Cookie" / "cookie" / "COOKIE") are redacted
 *  regardless of how the transport normalised them.
 */
PRIVATE json_t *redact_for_trace(json_t *jn)
{
    if(!jn) {
        return json_object();
    }
    json_t *out = json_deep_copy(jn);
    if(!json_is_object(out)) {
        return out;
    }
    static const char *secret_keys[] = {
        "password",
        "code",
        "code_verifier",
        "access_token",
        "refresh_token",
        "id_token",
        "client_secret",
        "cookie",       /* HTTP header may carry the same tokens */
        NULL
    };

    /*
     *  First pass: find every object key whose name matches one of
     *  secret_keys[] case-insensitively.  We snapshot the matched
     *  keys into a local buffer because json_object_set_new during
     *  json_object_foreach iteration is not safe.
     */
    char matched[16][64];
    int n_matched = 0;
    const char *key;
    json_t *value;
    json_object_foreach(out, key, value) {
        if(!json_is_string(value)) {
            continue;
        }
        for(int i = 0; secret_keys[i]; i++) {
            if(strcasecmp(key, secret_keys[i]) == 0) {
                if(n_matched < (int)(sizeof(matched)/sizeof(matched[0]))) {
                    snprintf(matched[n_matched], sizeof(matched[0]), "%s", key);
                    n_matched++;
                }
                break;
            }
        }
    }

    for(int i = 0; i < n_matched; i++) {
        json_t *v = json_object_get(out, matched[i]);
        if(!json_is_string(v)) {
            continue;
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "<%zu chars>",
            strlen(json_string_value(v)));
        json_object_set_new(out, matched[i], json_string(buf));
    }

    return out;
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
 *  Send an HTTP error JSON response and log it on the server.
 *
 *  Browser contract (see c_auth_bff.h for the full code catalogue):
 *      { "success":    false,
 *        "error_code": "<stable_snake_case_code>",
 *        "error":      "<english developer fallback>" }
 *
 *  The GUI MUST use `error_code` as its i18n translation key.  `error`
 *  is only a fallback and is also mirrored into the server log line.
 *
 *  Log severity is determined by HTTP status class:
 *    - 4xx  →  gobj_log_info  "BFF request rejected"
 *              Expected business outcomes: wrong password, expired
 *              session, malformed request, bad endpoint.  NOT an error
 *              — the server is doing exactly what it should.
 *    - 5xx  →  gobj_log_error "BFF server error"
 *              Something is genuinely broken: upstream down, config
 *              wrong, server overwhelmed.  The operator should look.
 ***************************************************************************/
PRIVATE void send_error_response(hgobj gobj, hgobj browser_src,
    int status_code, const char *status_text,
    const char *error_code, const char *error_msg,
    const char *extra_headers)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->st_bff_errors++;

    if(status_code >= 500) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "BFF server error",
            "status",       "%d", status_code,
            "status_text",  "%s", status_text ? status_text : "",
            "error_code",   "%s", error_code ? error_code : "",
            "error",        "%s", error_msg ? error_msg : "",
            "browser_src",  "%s", browser_src ? gobj_short_name(browser_src) : "",
            NULL
        );
    } else {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "BFF request rejected",
            "status",       "%d", status_code,
            "status_text",  "%s", status_text ? status_text : "",
            "error_code",   "%s", error_code ? error_code : "",
            "error",        "%s", error_msg ? error_msg : "",
            "browser_src",  "%s", browser_src ? gobj_short_name(browser_src) : "",
            NULL
        );
    }

    json_t *jn_body = json_pack("{s:b, s:s, s:s}",
        "success",    0,
        "error_code", error_code ? error_code : "unknown_error",
        "error",      error_msg ? error_msg : ""
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
    if(priv->q_count >= priv->queue_size) {
        priv->st_q_full_drops++;
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "BFF pending queue full",
            "queue_size",   "%d", priv->queue_size,
            NULL
        );
        return -1;
    }
    priv->queue[priv->q_tail] = *pa;
    priv->q_tail = (priv->q_tail + 1) % priv->queue_size;
    priv->q_count++;

    priv->st_requests_total++;
    if((uint64_t)priv->q_count > priv->st_q_max_seen) {
        priv->st_q_max_seen = priv->q_count;
    }
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
    priv->q_head = (priv->q_head + 1) % priv->queue_size;
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

    {
        /*
         *  Count any 2xx as success.  Keycloak's /token endpoint
         *  returns 200 on the happy path, but the 2xx range is the
         *  right semantic net — mirrors result_kc_logout below.
         */
        PRIVATE_DATA *priv = gobj_priv_data(gobj);
        if(status >= 200 && status < 300) {
            priv->st_kc_ok++;
        } else {
            priv->st_kc_errors++;
        }
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "BFF ← Keycloak: action=%s status=%d",
            action_name(action), status
        );
    }
    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        json_t *jn_full_body = kw_get_dict(gobj, kw, "body", NULL, 0);
        if(jn_full_body) {
            json_t *jn_redact = redact_for_trace(jn_full_body);
            gobj_trace_json(gobj, jn_redact, "BFF ← Keycloak body (redacted)");
            JSON_DECREF(jn_redact)
        }
    }

    /*
     *  Drop the reply if the connection we were serving is no longer
     *  the one plugged into this BFF.  Two cases collapse here into a
     *  single gen-mismatch check:
     *
     *    - Browser closed and no new connection came in yet
     *      → browser_alive_gen == 0
     *    - Browser closed AND a new connection opened on the same BFF
     *      (persistent_channels=true + fast reconnect)
     *      → browser_alive_gen != 0 but != task_gen
     *
     *  Either way the token belongs to a user who is no longer on the
     *  wire and must NOT be forwarded to whoever is on it now — that
     *  would leak one user's token to the next browser on the same
     *  BFF instance (test12_stale_reply exercises exactly this race).
     *
     *  Stats already reflect what Keycloak said (kc_ok / kc_errors
     *  bumped above), so charts stay truthful; only the forward to
     *  the wrong socket is skipped.  CONTINUE_TASK so the task
     *  finishes normally and ac_end_task runs its cleanup.
     */
    {
        PRIVATE_DATA *priv = gobj_priv_data(gobj);
        uint64_t task_gen = (uint64_t)kw_get_int(
            gobj, output_data, "_browser_gen", 0, 0);
        if(task_gen == 0 || task_gen != priv->browser_alive_gen) {
            priv->st_responses_dropped++;
            if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
                gobj_trace_msg(gobj,
                    "BFF browser gen mismatch — dropping response: "
                    "action=%s status=%d task_gen=%llu alive_gen=%llu",
                    action_name(action), status,
                    (unsigned long long)task_gen,
                    (unsigned long long)priv->browser_alive_gen
                );
            }
            KW_DECREF(kw)
            CONTINUE_TASK()
        }
    }

    char cors_hdrs[1024];
    build_cors_headers(gobj, origin, cors_hdrs, sizeof(cors_hdrs), FALSE);

    if(status != 200) {
        /*
         *  Map Keycloak failures to stable browser-facing error codes.
         *  Never leak the word "Keycloak" to the browser — that's an
         *  internal implementation detail.  The operator still gets
         *  full detail via gobj_log_error emitted by send_error_response.
         *
         *  Keycloak's /token endpoint replies with an RFC-6749 error
         *  envelope: { "error": "<rfc_code>", "error_description": "..." }.
         *  We parse that body when present to distinguish the real cause.
         *
         *  See the error-code catalogue at the top of c_auth_bff.h.
         */
        int         browser_status = 502;
        const char *browser_code   = "auth_unexpected_error";
        const char *browser_msg    = "Unexpected authentication error";

        json_t *jn_err_body = kw_get_dict(gobj, kw, "body", NULL, 0);
        const char *kc_err  = kw_get_str(gobj, jn_err_body, "error",             "", 0);
        const char *kc_desc = kw_get_str(gobj, jn_err_body, "error_description", "", 0);

        if(strcmp(kc_err, "invalid_grant") == 0 &&
                (status == 400 || status == 401)) {
            /*
             *  Keycloak packs several distinct outcomes under invalid_grant:
             *    - "Invalid user credentials"      → wrong user/pass
             *    - "Account is not fully set up"   → disabled/incomplete
             *    - "Account disabled"              → disabled
             *    - "Invalid refresh token"         → expired/rotated RT
             *    - "Token is not active"           → refresh_token expired
             *    - "Code not valid"                → PKCE code reused/expired
             *
             *  The right browser-facing mapping depends on BOTH the human
             *  description AND the action that triggered the Keycloak call:
             *
             *    * A disabled account is a disabled account regardless of
             *      whether we got there via /auth/login or /auth/refresh,
             *      so that check is action-agnostic.
             *
             *    * An invalid_grant from /auth/login means "user typed the
             *      wrong password" → invalid_credentials.
             *
             *    * An invalid_grant from /auth/refresh or /auth/callback
             *      means "the refresh_token or authorization code we were
             *      carrying is no longer valid" — NOT that the user typed
             *      anything wrong.  Mapping it to invalid_credentials would
             *      show "Usuario o contraseña incorrectos" after a silent
             *      background refresh, which is actively misleading.  Map
             *      to session_expired so the GUI renders "your session has
             *      expired, please log in again" instead.
             */
            if(strcasestr(kc_desc, "disabled") ||
               strcasestr(kc_desc, "not fully set up")) {
                browser_status = 403;
                browser_code   = "account_disabled";
                browser_msg    = "Account disabled or not fully configured";
            } else if(action == BFF_LOGIN) {
                browser_status = 401;
                browser_code   = "invalid_credentials";
                browser_msg    = "Invalid username or password";
            } else {
                /* BFF_REFRESH or BFF_CALLBACK */
                browser_status = 401;
                browser_code   = "session_expired";
                browser_msg    = "Session expired, please log in again";
            }
        } else if(status == 400 && strcmp(kc_err, "invalid_client") == 0) {
            browser_status = 500;
            browser_code   = "auth_config_error";
            browser_msg    = "Authentication configuration error";
        } else if(status == 429) {
            browser_status = 429;
            browser_code   = "auth_rate_limited";
            browser_msg    = "Too many login attempts, please try again later";
        } else if(status >= 500 && status < 600) {
            browser_status = 502;
            browser_code   = "auth_service_unavailable";
            browser_msg    = "Authentication service unavailable";
        }

        send_error_response(gobj, browser_src, browser_status,
            status_str(browser_status), browser_code, browser_msg, cors_hdrs);
        KW_DECREF(kw)
        STOP_TASK()
    }

    json_t *jn_body = kw_get_dict(gobj, kw, "body", NULL, KW_REQUIRED);
    if(!jn_body) {
        send_error_response(gobj, browser_src, 502, status_str(502),
            "auth_empty_response",
            "Empty response from authentication service", cors_hdrs);
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
    const char *cookie_domain_cfg = gobj_read_str_attr(gobj, "cookie_domain");
    const char *request_host = kw_get_str(gobj, output_data, "_host", "", 0);
    const char *cookie_domain = effective_cookie_domain(
        gobj, cookie_domain_cfg, request_host);
    char *at_cookie = make_set_cookie(COOKIE_NAME_AT, access_token,
        (int)expires_in, cookie_domain);
    char *rt_cookie = make_set_cookie(COOKIE_NAME_RT, refresh_token,
        (int)ref_exp_in, cookie_domain);

    /* extra_headers = CORS + two Set-Cookie lines */
    size_t extra_len = strlen(cors_hdrs) + strlen(at_cookie) + strlen(rt_cookie) + 4;
    char *extra = gbmem_malloc(extra_len);
    snprintf(extra, extra_len, "%s%s%s", cors_hdrs, at_cookie, rt_cookie);

    json_t *jn_resp;
    if(action == BFF_CALLBACK || action == BFF_LOGIN) {
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

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "BFF → browser: action=%s status=200 username=%s email=%s "
            "expires_in=%lld refresh_expires_in=%lld",
            action_name(action),
            username, email,
            (long long)expires_in, (long long)ref_exp_in
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
    build_path(resource, sizeof(resource), priv->path, "token", NULL);

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

    } else if(action == BFF_LOGIN) {
        const char *username = kw_get_str(gobj, output_data, "_username", "", 0);
        const char *password = kw_get_str(gobj, output_data, "_password", "", 0);

        jn_data = json_pack("{s:s, s:s, s:s, s:s}",
            "grant_type",    "password",
            "username",      username,
            "password",      password,
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

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "BFF action_call_keycloak: action=%s resource=%s",
            action_name(action), resource
        );
    }
    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        json_t *jn_redact = redact_for_trace(jn_data);
        gobj_trace_json(gobj, jn_redact, "BFF → Keycloak data (redacted)");
        JSON_DECREF(jn_redact)
    }

    json_t *query = json_pack("{s:s, s:s, s:s, s:o, s:o}",
        "method",   "POST",
        "resource", resource,
        "query",    "",
        "headers",  jn_headers,
        "data",     jn_data
    );
    gobj_send_event(priv->gobj_idprovider, EV_SEND_MESSAGE, query, gobj);

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
    build_path(resource, sizeof(resource), priv->path, "logout", NULL);

    json_t *jn_headers = json_pack("{s:s}", "Content-Type",
        "application/x-www-form-urlencoded");
    json_t *jn_data = json_pack("{s:s, s:s}",
        "refresh_token", rt,
        "client_id",     client_id
    );
    if(!empty_string(cs)) {
        json_object_set_new(jn_data, "client_secret", json_string(cs));
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "BFF action_kc_logout: resource=%s rt_len=%zu",
            resource, strlen(rt)
        );
    }
    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        json_t *jn_redact = redact_for_trace(jn_data);
        gobj_trace_json(gobj, jn_redact, "BFF → Keycloak logout data (redacted)");
        JSON_DECREF(jn_redact)
    }

    json_t *query = json_pack("{s:s, s:s, s:s, s:o, s:o}",
        "method",   "POST",
        "resource", resource,
        "query",    "",
        "headers",  jn_headers,
        "data",     jn_data
    );
    gobj_send_event(priv->gobj_idprovider, EV_SEND_MESSAGE, query, gobj);

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

    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);

    {
        /*
         *  Keycloak returns 204 No Content on a successful logout
         *  (RFC 6749 / OIDC RP-Initiated Logout), not 200.  Count any
         *  2xx response as success so the kc_ok / kc_errors counters
         *  reflect real Keycloak behaviour.
         */
        PRIVATE_DATA *priv = gobj_priv_data(gobj);
        if(status >= 200 && status < 300) {
            priv->st_kc_ok++;
        } else {
            priv->st_kc_errors++;
        }
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "BFF ← Keycloak logout: status=%d", status);
    }
    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        json_t *jn_full_body = kw_get_dict(gobj, kw, "body", NULL, 0);
        if(jn_full_body) {
            json_t *jn_redact = redact_for_trace(jn_full_body);
            gobj_trace_json(gobj, jn_redact, "BFF ← Keycloak logout body (redacted)");
            JSON_DECREF(jn_redact)
        }
    }

    /*
     *  Same browser-generation gate as result_token_response.  Drops
     *  the reply if the connection we were logging out of has been
     *  closed or replaced.  Stats already reflect Keycloak's reply;
     *  only the forward to the wrong socket is skipped.
     */
    {
        PRIVATE_DATA *priv = gobj_priv_data(gobj);
        uint64_t task_gen = (uint64_t)kw_get_int(
            gobj, output_data, "_browser_gen", 0, 0);
        if(task_gen == 0 || task_gen != priv->browser_alive_gen) {
            priv->st_responses_dropped++;
            if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
                gobj_trace_msg(gobj,
                    "BFF browser gen mismatch on logout reply — dropping: "
                    "status=%d task_gen=%llu alive_gen=%llu",
                    status,
                    (unsigned long long)task_gen,
                    (unsigned long long)priv->browser_alive_gen
                );
            }
            KW_DECREF(kw)
            CONTINUE_TASK()
        }
    }

    char cors_hdrs[1024];
    build_cors_headers(gobj, origin, cors_hdrs, sizeof(cors_hdrs), FALSE);

    if(browser_src) {
        const char *cookie_domain_cfg = gobj_read_str_attr(gobj, "cookie_domain");
        const char *request_host = kw_get_str(gobj, output_data, "_host", "", 0);
        const char *cookie_domain = effective_cookie_domain(
            gobj, cookie_domain_cfg, request_host);
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

        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj, "BFF → browser: logout status=200");
        }
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

    if(priv->processing || priv->q_count == 0) {
        return;
    }

    PENDING_AUTH *pa = dequeue(gobj);
    if(!pa) {
        return;
    }

    priv->processing = TRUE;

    /* Build Keycloak URL from parsed parts */
    char kc_token_url[PATH_MAX * 2];
    char kc_token_path[PATH_MAX];
    build_path(kc_token_path, sizeof(kc_token_path), priv->path, "token", NULL);
    snprintf(kc_token_url, sizeof(kc_token_url),
        "%s://%s%s%s%s",
        priv->schema,
        priv->host,
        empty_string(priv->port) ? "" : ":",
        empty_string(priv->port) ? "" : priv->port,
        kc_token_path
    );

    priv->st_kc_calls++;

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "BFF → Keycloak: action=%s url=%s queue_after=%d",
            action_name(pa->action), kc_token_url, priv->q_count
        );
    }

    /*
     *  Build the task output_data.  `_browser_gen` rides along with
     *  `_browser_src` so result_token_response / result_kc_logout can
     *  tell, when the async reply eventually arrives, whether the
     *  connection we dispatched this task for is still on the wire.
     */
    json_t *kw_output = json_pack("{s:I, s:I, s:i, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "_browser_src",    (json_int_t)(uintptr_t)pa->browser_src,
        "_browser_gen",    (json_int_t)pa->browser_gen,
        "_action",         (int)pa->action,
        "_code",           pa->code,
        "_code_verifier",  pa->code_verifier,
        "_redirect_uri",   pa->redirect_uri,
        "_username",       pa->username,
        "_password",       pa->password,
        "_refresh_token",  pa->refresh_token,
        "_origin",         pa->client_origin,
        "_host",           pa->client_host
    );

    json_t *kw_task;
    if(pa->action == BFF_CALLBACK || pa->action == BFF_LOGIN || pa->action == BFF_REFRESH) {
        kw_task = json_pack(
            "{s:o, s:o, s:o, s:["
                "{s:s, s:s},"
                "{s:s, s:s}"
            "]}",
            "gobj_jobs",    json_integer((json_int_t)(uintptr_t)gobj),
            "gobj_results", json_integer((json_int_t)(uintptr_t)priv->gobj_idprovider),
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
            "gobj_results", json_integer((json_int_t)(uintptr_t)priv->gobj_idprovider),
            "output_data",  kw_output,
            "jobs",
                "exec_action", "action_kc_logout",
                "exec_result", "result_kc_logout",
                "exec_action", "action_done",
                "exec_result", "result_done"
        );
    }

    hgobj gobj_task = gobj_create(priv->idp_name, C_TASK, kw_task, gobj);
    gobj_subscribe_event(gobj_task, EV_END_TASK, NULL, gobj);
    gobj_set_volatil(gobj_task, TRUE);
    priv->gobj_task = gobj_task;

    gobj_start(gobj_task);

    /*
     *  Remember the browser this task is serving so ac_kc_watchdog
     *  can 504 it without having to reach into the task's output_data.
     *  Capture the browser generation too so the watchdog can
     *  short-circuit if the connection has been closed or replaced
     *  between dispatch and timeout.
     */
    priv->active_browser_src = pa->browser_src;
    priv->active_task_gen    = pa->browser_gen;
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
 *  Connection from browser or IdProvider
 *
 *  Bumps the browser generation counter and stores the new value as
 *  `browser_alive_gen`.  Every task that gets enqueued while this is
 *  the current gen carries it through its own output_data, and
 *  result_token_response / result_kc_logout compare the two before
 *  forwarding the Keycloak reply — see the PRIVATE_DATA comment on
 *  the generation scheme for the full rationale.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gclass_name_t gclass_src_name = gobj_gclass_name(src);
    if(gclass_src_name == C_PROT_HTTP_CL) {
        /*
         *  Connection from IdProvider
         */

    } else if(gclass_src_name == C_PROT_HTTP_SR) {
        /*
         *  Connection from browser
         */
        priv->browser_gen_counter++;
        priv->browser_alive_gen = priv->browser_gen_counter;

        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj,
                "BFF connection OPEN from %s (gen=%llu)",
                gobj_short_name(src),
                (unsigned long long)priv->browser_alive_gen
            );
        }
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "What merde is message from?",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Browser HTTP connection closed (before we could respond, or after).
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gclass_name_t gclass_src_name = gobj_gclass_name(src);
    if(gclass_src_name == C_PROT_HTTP_CL) {
        /*
         *  Connection from IdProvider
         */

    } else if(gclass_src_name == C_PROT_HTTP_SR) {
        /*
         *  Connection from browser
         */
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj,
                "BFF connection CLOSE from %s (gen=%llu q_count=%d processing=%d)",
                gobj_short_name(src),
                (unsigned long long)priv->browser_alive_gen,
                priv->q_count, priv->processing
            );
        }

        priv->browser_alive_gen = 0;

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "What merde is message from?",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Incoming HTTP request from browser.
 *  kw: { url, request_method, headers (json), body (json) }
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    int method = (int)kw_get_int(gobj, kw, "request_method", 0, 0);
    const char *url    = kw_get_str(gobj, kw, "url", "", 0);
    json_t *jn_headers = kw_get_dict(gobj, kw, "headers", NULL, 0);
    json_t *jn_body    = kw_get_dict(gobj, kw, "body",    NULL, 0);

    const char *origin = jn_headers ?
        kw_get_str(gobj, jn_headers, "ORIGIN", "", 0) : "";
    const char *cookie_hdr = jn_headers ?
        kw_get_str(gobj, jn_headers, "COOKIE", "", 0) : "";
    const char *host_hdr = jn_headers ?
        kw_get_str(gobj, jn_headers, "HOST", "", 0) : "";

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "BFF request: method=%d url=%s host=%s origin=%s from=%s",
            method, url, host_hdr, origin, gobj_short_name(src)
        );
    }
    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        if(jn_headers) {
            json_t *jn_redact = redact_for_trace(jn_headers);
            gobj_trace_json(gobj, jn_redact, "BFF request headers");
            JSON_DECREF(jn_redact)
        }
        if(jn_body) {
            json_t *jn_redact = redact_for_trace(jn_body);
            gobj_trace_json(gobj, jn_redact, "BFF request body (redacted)");
            JSON_DECREF(jn_redact)
        }
    }

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
        send_error_response(gobj, src, 405, "405 Method Not Allowed",
            "method_not_allowed", "Only POST is allowed", cors_hdrs);
        KW_DECREF(kw)
        return 0;
    }

    PENDING_AUTH pa;
    memset(&pa, 0, sizeof(pa));
    pa.browser_src = src;
    /*
     *  Capture the current browser generation so result_* / watchdog
     *  can tell, when the async reply eventually lands, whether the
     *  connection this task was dispatched for is still the one
     *  plugged into the BFF.  See the PRIVATE_DATA comment on
     *  browser_alive_gen for the full scheme.
     */
    {
        PRIVATE_DATA *priv_for_gen = gobj_priv_data(gobj);
        pa.browser_gen = priv_for_gen->browser_alive_gen;
    }
    snprintf(pa.client_origin, sizeof(pa.client_origin), "%s", origin);
    snprintf(pa.client_host, sizeof(pa.client_host), "%s", host_hdr);

    if(strcmp(url, "/auth/callback") == 0) {
        /* POST /auth/callback  { code, code_verifier, redirect_uri } */
        if(!jn_body) {
            send_error_response(gobj, src, 400, status_str(400),
                "missing_body", "Missing JSON body", cors_hdrs);
            KW_DECREF(kw)
            return 0;
        }
        const char *code    = kw_get_str(gobj, jn_body, "code",          "", 0);
        const char *cv      = kw_get_str(gobj, jn_body, "code_verifier", "", 0);
        const char *ruri    = kw_get_str(gobj, jn_body, "redirect_uri",  "", 0);

        if(empty_string(code) || empty_string(cv) || empty_string(ruri)) {
            send_error_response(gobj, src, 400, status_str(400),
                "missing_params",
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
            send_error_response(gobj, src, 400, status_str(400),
                "redirect_uri_not_allowed", "redirect_uri not allowed", cors_hdrs);
            KW_DECREF(kw)
            return 0;
        }

        pa.action = BFF_CALLBACK;
        snprintf(pa.code,          sizeof(pa.code),          "%s", code);
        snprintf(pa.code_verifier, sizeof(pa.code_verifier), "%s", cv);
        snprintf(pa.redirect_uri,  sizeof(pa.redirect_uri),  "%s", ruri);
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj,
                "BFF /auth/callback: code_len=%zu cv_len=%zu redirect_uri=%s",
                strlen(code), strlen(cv), ruri
            );
        }

    } else if(strcmp(url, "/auth/login") == 0) {
        /* POST /auth/login  { username, password } — Direct Access Grant (ROPC) */
        if(!jn_body) {
            send_error_response(gobj, src, 400, status_str(400),
                "missing_body", "Missing JSON body", cors_hdrs);
            KW_DECREF(kw)
            return 0;
        }
        const char *username = kw_get_str(gobj, jn_body, "username", "", 0);
        const char *password = kw_get_str(gobj, jn_body, "password", "", 0);

        if(empty_string(username) || empty_string(password)) {
            send_error_response(gobj, src, 400, status_str(400),
                "missing_params",
                "username and password are required", cors_hdrs);
            KW_DECREF(kw)
            return 0;
        }

        pa.action = BFF_LOGIN;
        snprintf(pa.username, sizeof(pa.username), "%s", username);
        snprintf(pa.password, sizeof(pa.password), "%s", password);
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj,
                "BFF /auth/login: username=%s password=<%zu chars>",
                username, strlen(password)
            );
        }

    } else if(strcmp(url, "/auth/refresh") == 0) {
        /* POST /auth/refresh  (reads httpOnly cookie) */
        char rt[4096];
        if(!extract_cookie(cookie_hdr, COOKIE_NAME_RT, rt, sizeof(rt)) ||
                empty_string(rt)) {
            send_error_response(gobj, src, 401, status_str(401),
                "missing_refresh_token",
                "Missing refresh_token cookie", cors_hdrs);
            KW_DECREF(kw)
            return 0;
        }
        pa.action = BFF_REFRESH;
        snprintf(pa.refresh_token, sizeof(pa.refresh_token), "%s", rt);
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj, "BFF /auth/refresh: rt_len=%zu", strlen(rt));
        }

    } else if(strcmp(url, "/auth/logout") == 0) {
        /* POST /auth/logout  (reads httpOnly cookie) */
        char rt[4096];
        extract_cookie(cookie_hdr, COOKIE_NAME_RT, rt, sizeof(rt));
        pa.action = BFF_LOGOUT;
        snprintf(pa.refresh_token, sizeof(pa.refresh_token), "%s", rt);
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj, "BFF /auth/logout: rt_len=%zu", strlen(rt));
        }

    } else {
        send_error_response(gobj, src, 404, "404 Not Found",
            "unknown_endpoint", "Unknown endpoint", cors_hdrs);
        KW_DECREF(kw)
        return 0;
    }

    if(enqueue(gobj, &pa) < 0) {
        send_error_response(gobj, src, 503, "503 Service Unavailable",
            "server_busy", "Server busy, retry in a moment", cors_hdrs);
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
PRIVATE int ac_end_task(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Disarm the watchdog via flag — do NOT clear_timeout0 here.
     *  clear_timeout0 posts a CANCEL SQE and the timer enters
     *  CANCELING state.  process_next() runs synchronously below for
     *  any queued request and would call set_timeout0, which fails
     *  with "cannot start timer: is CANCELING" until the cancel CQE
     *  drains.  Instead, the live timer is left running with its
     *  remaining duration; ac_kc_watchdog ignores fires when
     *  kc_watchdog_armed==FALSE, and the next process_next() either
     *  re-arms it (RUNNING → RUNNING via timerfd_settime, no cancel)
     *  or leaves it to fire harmlessly into the disarmed handler.
     */
    priv->active_browser_src = NULL;
    priv->active_task_gen    = 0;

    /*
     *  Tear down the C_TASK.  Two paths land here:
     *    - Natural completion: c_task::stop_task synchronously publishes
     *      EV_END_TASK BEFORE calling gobj_stop on itself, so we get the
     *      event with src == priv->gobj_task while it is still RUNNING.
     *      We must NOT stop it here — c_task will do it itself the
     *      moment we return, and a double-stop logs "GObj NOT RUNNING".
     *    - Watchdog (ac_kc_watchdog → gobj_send_event(EV_END_TASK, src=gobj)):
     *      the task is still in the middle of waiting for Keycloak and
     *      nobody is going to stop it for us.  src is c_auth_bff itself
     *      (not the task), so force-stop here.  EV_STOPPED then chains
     *      to ac_stopped which destroys the volatil task.
     */
    if(src != priv->gobj_task &&
            priv->gobj_task && gobj_is_running(priv->gobj_task)) {
        gobj_stop(priv->gobj_task);
    }
    priv->gobj_task = NULL;  /* ac_stopped will destroy it */

    priv->processing = FALSE;

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "BFF Keycloak task ended; pending=%d", priv->q_count);
    }

    KW_DECREF(kw)

    /* Immediately process next pending request if any */
    process_next(gobj);

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
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_stats   = mt_stats,
};

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
        {EV_ON_OPEN,    ac_on_open,         0},
        {EV_ON_MESSAGE, ac_on_message,      0},
        {EV_ON_CLOSE,   ac_on_close,        0},
        {EV_END_TASK,   ac_end_task,        0},
        {EV_STOPPED,    ac_stopped,         0},
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
        0,                  /* authz_table */
        command_table,
        s_user_trace_level,
        0                   /* gclass_flag */
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
