/****************************************************************************
 *          C_MOCK_KEYCLOAK.C
 *
 *          Test-only mock of the Keycloak token endpoint.  See the
 *          companion .h for the public surface and the per-attr protocol.
 *
 *          The gclass sits under a C_CHANNEL in the test yuno's __idp_side__
 *          IOGate, with a C_PROT_HTTP_SR child underneath.  The http_sr
 *          automatically subscribes to its parent (this gclass) so we
 *          receive every parsed request as EV_ON_MESSAGE; we synthesise a
 *          token envelope and send it back via EV_SEND_MESSAGE.
 *
 *          JWT signing uses libjwt with a fixed HS256 test key installed
 *          once per process via mock_keycloak_init_jwk().  The BFF under
 *          test does not verify signatures today — it only base64url
 *          decodes the payload segment — but the tokens are real so the
 *          test stays correct against any future BFF that adds verify.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <time.h>

#include <yunetas.h>
#include <jwt.h>                /* libjwt — not part of the yunetas.h umbrella */

#include "c_mock_keycloak.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
/*
 *  Fixed test JWK (HS256, symmetric).  The `k` value is base64url of the
 *  literal ASCII string "yuneta-auth-bff-test-key-0123456" (32 bytes).
 *  A stable, hard-coded key is intentional: tests must be deterministic
 *  and reproducible without touching the environment.
 */
PRIVATE const char *TEST_JWK_JSON =
    "{"
        "\"kty\":\"oct\","
        "\"k\":\"eXVuZXRhLWF1dGgtYmZmLXRlc3Qta2V5LTAxMjM0NTY\","
        "\"alg\":\"HS256\","
        "\"use\":\"sig\""
    "}";

/***************************************************************************
 *              Process-wide JWK state
 ***************************************************************************/
PRIVATE jwk_set_t         *s_jwk_set   = NULL;
PRIVATE const jwk_item_t  *s_hs256_key = NULL;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE char *make_signed_token(const char *username, const char *email, int ttl, int pad);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name------------flag----default-description*/
SDATA (DTP_INTEGER,     "return_status",    SDF_RD, "200",  "HTTP status code to return for token requests"),
SDATA (DTP_STRING,      "username",         SDF_RD, "mockuser", "preferred_username claim embedded in the access_token"),
SDATA (DTP_STRING,      "email",            SDF_RD, "mockuser@example.com", "email claim embedded in the access_token"),
SDATA (DTP_INTEGER,     "access_token_ttl", SDF_RD, "300",  "seconds: exp - iat for access_token"),
SDATA (DTP_INTEGER,     "refresh_token_ttl",SDF_RD, "1800", "seconds: exp - iat for refresh_token"),
SDATA (DTP_INTEGER,     "access_token_pad", SDF_RD, "0",    "If > 0, the access_token carries a `pad` claim of that many bytes (the token of a user with many roles)"),
SDATA (DTP_INTEGER,     "access_token_len", SDF_RD, "0",    "If > 0, the access_token is made exactly that long by lengthening its signature (the BFF does not verify it)"),
SDATA (DTP_INTEGER,     "refresh_token_len",SDF_RD, "0",    "If > 0, the refresh_token is made exactly that long by lengthening its signature"),
SDATA (DTP_JSON,        "override_body",    SDF_RD, "{}",   "If non-empty, sent verbatim instead of the synthesised token envelope"),
SDATA (DTP_DICT,        "body_patch",       SDF_RD, "{}",   "Keys set over the synthesised token envelope; a key whose value is null is removed from it (an answer with no refresh_token, a token of another type)"),
SDATA (DTP_STRING,      "raw_content_type", SDF_RD, "",     "If non-empty, a token request is answered 200 with this Content-Type and `raw_body` as its bytes, written as they are (an empty body, a body that is not JSON, a JSON that is not an object)"),
SDATA (DTP_STRING,      "raw_body",         SDF_RD, "",     "The body of a raw answer (see raw_content_type); empty: Content-Length 0"),
SDATA (DTP_JSON,        "override_discovery_body", SDF_RD, "{}", "If non-empty, sent verbatim instead of the synthesised /.well-known body (lets a test return a body missing required endpoints — exercises save_oidc_discovery's failure path)"),
SDATA (DTP_BOOLEAN,     "silent_discovery", SDF_RD, "0",    "If true, the /.well-known GET is taken and never answered (an IdP that accepts the connection and stays silent: exercises the discovery timeout)"),
SDATA (DTP_INTEGER,     "latency_ms",       SDF_RD, "0",    "If > 0, defer each response by this many ms via an internal C_TIMER0 child (high-resolution). Single pending slot — not re-entrant."),
SDATA (DTP_STRING,      "retry_after",      SDF_RD, "",     "If non-empty, the answers to /token carry a Retry-After header with this value"),
SDATA (DTP_STRING,      "raw_answer",       SDF_RD, "",     "If non-empty, a /token or /logout request is answered with these bytes as they are, status line and headers included (a head too long, a response that breaks the protocol)"),
SDATA (DTP_BOOLEAN,     "drop_token_request", SDF_RD, "0",  "If true, the next /token request is not answered: its connection is dropped (an IdP that restarts, or whose answer the client cannot parse); cleared when used"),
SDATA (DTP_STRING,      "last_form",        SDF_RD, "",     "The body of the last /token or /logout request, as it arrived (the form of the BFF); a test clears it before a step"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"messages",    "Trace mock Keycloak request/response flow"},
    {0, 0}
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    /* Stats */
    uint64_t    st_requests_received;
    uint64_t    st_responses_sent;
    uint64_t    st_token_requests;
    uint64_t    st_logout_requests;
    uint64_t    st_discovery_requests;
    uint64_t    st_unknown_requests;
    uint64_t    st_deferred_responses;
    uint64_t    st_pending_cancelled;

    /*
     *  Deferred response slot (used when latency_ms > 0).
     *  Only one pending response at a time — not re-entrant.  A
     *  second request arriving with a pending slot still occupied is
     *  treated as a test-harness bug and logged via gobj_log_error.
     */
    hgobj       timer;
    hgobj       pending_src;                /* c_prot_http_sr to reply to */
    int         pending_status;
    char        pending_status_text[64];
    json_t     *pending_body;               /* owned, decref in send/cancel */
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
     *  Timer child used by the latency_ms feature.  Always created so
     *  respond_or_defer() can unconditionally call set_timeout0(); when
     *  latency_ms==0 it is never armed and costs nothing.
     *
     *  C_TIMER0 (high-resolution io_uring timer), NOT C_TIMER, because
     *  C_TIMER is documented as "ACCURACY IN SECONDS" — it rides the
     *  yuno's 1-s periodic tick, so a 500 ms latency would round up to
     *  the next second boundary and align with whatever else fires on
     *  the same tick.  test9_browser_cancel needs the cancel to land
     *  BEFORE the latency response, which only works with millisecond
     *  accuracy.
     *
     *  C_PROT_HTTP_SR (our child, plugged in by the yuno tree) auto-
     *  subscribes to its parent in its own mt_create, so we do not have
     *  to do anything else here: EV_ON_MESSAGE arrives naturally.
     */
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /* If a response was deferred and never sent, drop its body. */
    JSON_DECREF(priv->pending_body)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 *
 *  Three things to tear down here:
 *    1) the internal C_TIMER child (own pure child);
 *    2) any pending deferred response (body decref, slot cleared);
 *    3) our own bottom (the inbound C_PROT_HTTP_SR + C_TCP).  C_CHANNEL
 *       stops its bottom (us) but does not recurse, so the inbound
 *       stack would otherwise stay RUNNING during yuno teardown.
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);

    if(priv->pending_body) {
        JSON_DECREF(priv->pending_body)
        priv->pending_src = NULL;
    }

    hgobj bottom = gobj_bottom_gobj(gobj);
    if(bottom && gobj_is_running(bottom)) {
        gobj_stop_tree(bottom);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method stats
 ***************************************************************************/
PRIVATE json_t *mt_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(stats && strcmp(stats, "__reset__") == 0) {
        priv->st_requests_received  = 0;
        priv->st_responses_sent     = 0;
        priv->st_token_requests     = 0;
        priv->st_logout_requests    = 0;
        priv->st_discovery_requests = 0;
        priv->st_unknown_requests   = 0;
        priv->st_deferred_responses = 0;
        priv->st_pending_cancelled  = 0;
    }

    json_t *jn_data = json_pack("{s:I, s:I, s:I, s:I, s:I, s:I, s:I, s:I}",
        "requests_received",    (json_int_t)priv->st_requests_received,
        "responses_sent",       (json_int_t)priv->st_responses_sent,
        "token_requests",       (json_int_t)priv->st_token_requests,
        "logout_requests",      (json_int_t)priv->st_logout_requests,
        "discovery_requests",   (json_int_t)priv->st_discovery_requests,
        "unknown_requests",     (json_int_t)priv->st_unknown_requests,
        "deferred_responses",   (json_int_t)priv->st_deferred_responses,
        "pending_cancelled",    (json_int_t)priv->st_pending_cancelled
    );

    KW_DECREF(kw)
    return build_stats_response(gobj, 0, 0, 0, jn_data);
}




            /***************************
             *      Helpers
             ***************************/




/***************************************************************************
 *  A 200 whose Content-Type and body are written as they are, straight to
 *  the transport below the http_sr child that delivered the request: the
 *  http_sr only sends a body it can dump as json.
 ***************************************************************************/
PRIVATE void mk_send_raw(hgobj gobj, hgobj browser_src, const char *content_type,
    const char *body)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t body_len = strlen(body);
    gbuffer_t *gbuf = gbuffer_create(256 + body_len, 256 + body_len);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_MEMORY,
            "msg",      "%s", "mock-kc: no memory for a raw answer",
            NULL
        );
        return;
    }
    gbuffer_printf(gbuf,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n\r\n",
        content_type,
        (int)body_len
    );
    if(body_len > 0) {
        gbuffer_append(gbuf, (void *)body, body_len);
    }

    json_t *kw_tx = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    gobj_send_event(gobj_bottom_gobj(browser_src), EV_TX_DATA, kw_tx, gobj);
    priv->st_responses_sent++;

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "mock-kc → %s: raw 200, %s, %d bytes",
            gobj_short_name(browser_src), content_type, (int)body_len
        );
    }
}

/***************************************************************************
 *  The bytes of `raw_answer` written as they are, straight to the
 *  transport below the http_sr child that delivered the request
 ***************************************************************************/
PRIVATE void mk_send_raw_answer(hgobj gobj, hgobj browser_src, const char *raw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t len = strlen(raw);
    gbuffer_t *gbuf = gbuffer_create(len, len);
    if(!gbuf) {
        return;  // Error already logged
    }
    gbuffer_append(gbuf, (void *)raw, len);
    json_t *kw_tx = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    gobj_send_event(gobj_bottom_gobj(browser_src), EV_TX_DATA, kw_tx, gobj);
    priv->st_responses_sent++;
}

/***************************************************************************
 *  Keep the body of a request (a form, not JSON: it comes in a gbuffer)
 *  in `last_form`
 ***************************************************************************/
PRIVATE void save_form(hgobj gobj, json_t *kw)
{
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(!gbuf) {
        gobj_write_str_attr(gobj, "last_form", "");
        return;
    }
    char *form = gbmem_strndup(gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf));
    if(!form) {
        return;  // Error already logged
    }
    gobj_write_str_attr(gobj, "last_form", form);
    GBMEM_FREE(form)
}

/***************************************************************************
 *  Send a JSON response back to the http_sr child that delivered `src`.
 *  Mirrors c_auth_bff::send_json_response: status string, headers string,
 *  body JSON, EV_SEND_MESSAGE on the http_sr gobj (`src`).
 ***************************************************************************/
PRIVATE void mk_send_json(hgobj gobj, hgobj browser_src, int status,
    const char *status_text, json_t *jn_body)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char headers[256] = "";
    const char *retry_after = gobj_read_str_attr(gobj, "retry_after");
    if(!empty_string(retry_after)) {
        snprintf(headers, sizeof(headers), "Retry-After: %s\r\n", retry_after);
    }
    json_t *kw_resp = json_pack("{s:s, s:s, s:O}",
        "code",     status_text,
        "headers",  headers,
        "body",     jn_body
    );
    gobj_send_event(browser_src, EV_SEND_MESSAGE, kw_resp, gobj);
    priv->st_responses_sent++;

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "mock-kc → %s: status=%d", gobj_short_name(browser_src), status
        );
    }
    (void)status;
}

/***************************************************************************
 *  Either send the response now (latency_ms == 0) or stash it in the
 *  pending slot and arm the timer (latency_ms > 0).
 *
 *  Takes ownership of `jn_body` (always decrefs, even in the deferred
 *  path — it's held in priv->pending_body until ac_timer fires).
 ***************************************************************************/
PRIVATE void respond_or_defer(hgobj gobj, hgobj browser_src, int status,
    const char *status_text, json_t *jn_body)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int latency_ms = (int)gobj_read_integer_attr(gobj, "latency_ms");
    if(latency_ms <= 0) {
        mk_send_json(gobj, browser_src, status, status_text, jn_body);
        JSON_DECREF(jn_body)
        return;
    }

    /*
     *  Single-slot deferred response.  Re-entrance (a second request
     *  arriving while a pending response is still on the clock) is a
     *  test-harness bug, not a feature: flag it and drop the old one
     *  so the new request still gets served.
     */
    if(priv->pending_body) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "mock-kc deferred slot already occupied — single-slot only",
            "previous_src", "%s", priv->pending_src ? gobj_short_name(priv->pending_src) : "",
            "new_src",      "%s", browser_src ? gobj_short_name(browser_src) : "",
            NULL
        );
        JSON_DECREF(priv->pending_body)
        priv->pending_src = NULL;
        clear_timeout0(priv->timer);
    }

    priv->pending_src     = browser_src;
    priv->pending_status  = status;
    snprintf(priv->pending_status_text, sizeof(priv->pending_status_text),
        "%s", status_text ? status_text : "");
    priv->pending_body    = jn_body;        /* takes ownership */
    priv->st_deferred_responses++;

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "mock-kc deferred: status=%d latency=%d ms src=%s",
            status, latency_ms, gobj_short_name(browser_src)
        );
    }

    set_timeout0(priv->timer, latency_ms);
}

/***************************************************************************
 *  A token made exactly `len` bytes long (when len > 0) by lengthening its
 *  signature: its claims stay readable, and the BFF never verifies it.
 *  Takes ownership of `token`; returns a gbmem string, or NULL (logged).
 ***************************************************************************/
PRIVATE char *stretch_token(hgobj gobj, char *token, int len)
{
    if(!token || len <= 0) {
        return token;   // Error already logged when !token
    }
    size_t cur = strlen(token);
    if(cur > (size_t)len) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "mock-kc token longer than the length asked",
            "token_len","%d", (int)cur,
            "len",      "%d", len,
            NULL
        );
        GBMEM_FREE(token)
        return NULL;
    }
    char *stretched = gbmem_malloc((size_t)len + 1);
    if(!stretched) {
        GBMEM_FREE(token)
        return NULL;    // Error already logged
    }
    memcpy(stretched, token, cur);
    memset(stretched + cur, 'A', (size_t)len - cur);
    stretched[len] = '\0';
    GBMEM_FREE(token)
    return stretched;
}

/***************************************************************************
 *  Build the synthesised Keycloak token envelope or return the
 *  override_body as-is.  Returns a new json_t owned by the caller.
 ***************************************************************************/
PRIVATE json_t *build_token_envelope(hgobj gobj)
{
    /*
     *  Operator-supplied override wins: used by negative-path tests that
     *  want to exercise malformed / empty-body branches in the BFF.
     */
    json_t *override = gobj_read_json_attr(gobj, "override_body");
    if(json_is_object(override) && json_object_size(override) > 0) {
        return json_deep_copy(override);
    }

    const char *username = gobj_read_str_attr(gobj, "username");
    const char *email    = gobj_read_str_attr(gobj, "email");
    int at_ttl           = (int)gobj_read_integer_attr(gobj, "access_token_ttl");
    int rt_ttl           = (int)gobj_read_integer_attr(gobj, "refresh_token_ttl");

    int at_pad           = (int)gobj_read_integer_attr(gobj, "access_token_pad");
    int at_len           = (int)gobj_read_integer_attr(gobj, "access_token_len");
    int rt_len           = (int)gobj_read_integer_attr(gobj, "refresh_token_len");

    char *access  = stretch_token(gobj, make_signed_token(username, email, at_ttl, at_pad), at_len);
    char *refresh = stretch_token(gobj, make_signed_token(username, email, rt_ttl, 0), rt_len);

    json_t *jn = json_pack("{s:s, s:s, s:i, s:i, s:s, s:s}",
        "access_token",         access  ? access  : "",
        "refresh_token",        refresh ? refresh : "",
        "expires_in",           at_ttl,
        "refresh_expires_in",   rt_ttl,
        "token_type",           "Bearer",
        "scope",                "openid email profile"
    );

    /*
     *  Tokens come from libjwt, which we wired to gbmem in
     *  mock_keycloak_init_jwk(), so they must be freed with GBMEM_FREE.
     */
    GBMEM_FREE(access)
    GBMEM_FREE(refresh)

    json_t *patch = gobj_read_json_attr(gobj, "body_patch");
    const char *key;
    json_t *value;
    json_object_foreach(patch, key, value) {
        if(json_is_null(value)) {
            json_object_del(jn, key);
        } else {
            json_object_set(jn, key, value);
        }
    }
    return jn;
}




            /***************************
             *      JWT signing
             ***************************/




/***************************************************************************
 *  One-time init of the process-wide HS256 test key.
 ***************************************************************************/
PUBLIC int mock_keycloak_init_jwk(void)
{
    if(s_jwk_set) {
        return 0;   /* already initialised */
    }

    /*
     *  Align libjwt's allocator with gbmem (which jansson is already
     *  using via entry_point.c).  Without this, jwt_encode() frees a
     *  jansson buffer (gbmem) through system free() and aborts.
     *  c_authz does the same call when it is loaded; this test doesn't
     *  pull c_authz in, so we have to do it ourselves.
     */
    sys_malloc_fn_t  malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t  calloc_func;
    sys_free_fn_t    free_func;
    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    jwt_set_alloc(malloc_func, free_func);

    s_jwk_set = jwks_create(TEST_JWK_JSON);
    if(!s_jwk_set) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "jwks_create failed for test JWK",
            NULL
        );
        return -1;
    }
    s_hs256_key = jwks_item_get(s_jwk_set, 0);
    if(!s_hs256_key) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "jwks_item_get(0) returned NULL",
            NULL
        );
        jwks_free(s_jwk_set);
        s_jwk_set = NULL;
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Tear down the process-wide HS256 test key.
 ***************************************************************************/
PUBLIC void mock_keycloak_end_jwk(void)
{
    if(s_jwk_set) {
        jwks_free(s_jwk_set);
        s_jwk_set = NULL;
        s_hs256_key = NULL;
    }
}

/***************************************************************************
 *  Build a signed HS256 JWT with preferred_username / email / iat / exp,
 *  and a `pad` claim of `pad` bytes when pad > 0.
 *  Returns a gbmem-allocated string owned by the caller — free with
 *  GBMEM_FREE, NOT system free().  libjwt's allocator was rewired to
 *  gbmem in mock_keycloak_init_jwk().
 ***************************************************************************/
PRIVATE char *make_signed_token(const char *username, const char *email, int ttl, int pad)
{
    if(!s_hs256_key) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "mock_keycloak_init_jwk() not called",
            NULL
        );
        return NULL;
    }

    jwt_builder_t *b = jwt_builder_new();
    if(!b) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_MEMORY,
            "msg",      "%s", "jwt_builder_new failed",
            NULL
        );
        return NULL;
    }

    if(jwt_builder_setkey(b, JWT_ALG_HS256, s_hs256_key) != 0) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "jwt_builder_setkey failed",
            NULL
        );
        jwt_builder_free(b);
        return NULL;
    }

    jwt_value_t jval;
    time_t now = time(NULL);

    jwt_set_SET_STR(&jval, "preferred_username", username ? username : "");
    jwt_builder_claim_set(b, &jval);

    jwt_set_SET_STR(&jval, "email", email ? email : "");
    jwt_builder_claim_set(b, &jval);

    jwt_set_SET_INT(&jval, "iat", (jwt_long_t)now);
    jwt_builder_claim_set(b, &jval);

    jwt_set_SET_INT(&jval, "exp", (jwt_long_t)(now + ttl));
    jwt_builder_claim_set(b, &jval);

    if(pad > 0) {
        char *padding = gbmem_malloc((size_t)pad + 1);
        if(!padding) {
            jwt_builder_free(b);
            return NULL;    // Error already logged
        }
        memset(padding, 'x', (size_t)pad);
        padding[pad] = '\0';
        jwt_set_SET_STR(&jval, "pad", padding);
        jwt_builder_claim_set(b, &jval);
        GBMEM_FREE(padding)
    }

    char *token = jwt_builder_generate(b);
    jwt_builder_free(b);

    if(!token) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "jwt_builder_generate returned NULL",
            NULL
        );
    }
    return token;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Parsed HTTP request arrived from our child C_PROT_HTTP_SR.
 *
 *  `src` is the http_sr gobj itself — we reply to it with EV_SEND_MESSAGE.
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->st_requests_received++;

    const char *url = kw_get_str(gobj, kw, "url", "", 0);
    int method      = (int)kw_get_int(gobj, kw, "request_method", 0, 0);

    /*
     *  ghttp_parser hands non-JSON bodies over via the "gbuffer" key
     *  in kw.  We never inspect the body (routing is URL-based) and we
     *  must NOT manually decref it: "gbuffer" is registered as a kwid
     *  binary type (gobj.c:560), so KW_DECREF below frees it for us.
     *  Manual decref here causes a double-free / "BAD gbuf_decref()".
     */

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "mock-kc ← %s: method=%d url=%s",
            gobj_short_name(src), method, url
        );
    }

    /*
     *  Route by url suffix.  The BFF posts to
     *    /realms/<realm>/protocol/openid-connect/token
     *    /realms/<realm>/protocol/openid-connect/logout
     *  and GETs
     *    /realms/<realm>/.well-known/openid-configuration
     *  so a plain strstr is enough for the test.
     */
    if(strstr(url, "/.well-known/openid-configuration")) {
        priv->st_discovery_requests++;
        if(gobj_read_bool_attr(gobj, "silent_discovery")) {
            if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
                gobj_trace_msg(gobj,
                    "mock-kc ← %s: discovery left unanswered", gobj_short_name(src)
                );
            }
            KW_DECREF(kw)
            return 0;
        }
        /*
         *  OIDC discovery: derive issuer + endpoints from the request's
         *  Host header so the BFF can reach us back on the same socket.
         *
         *  Always answered synchronously, regardless of latency_ms.
         *  latency_ms emulates a slow /token endpoint (the test's
         *  subject-under-test); discovery is BFF startup wiring and
         *  must not contend for the single pending-response slot, or
         *  the real flow would race against it (test7: bumps
         *  responses_sent/deferred_responses; test8: holds the BFF in
         *  !discovery_done while POSTs pile up in dl_pending).
         */
        json_t *jn_body;
        json_t *override_discovery = gobj_read_json_attr(gobj, "override_discovery_body");
        if(json_is_object(override_discovery) && json_object_size(override_discovery) > 0) {
            /* Tests use this to return a discovery body that omits one
             * of the required endpoints, exercising the
             * save_oidc_discovery failure path. */
            jn_body = json_deep_copy(override_discovery);
        } else {
            json_t *jn_headers = kw_get_dict(gobj, kw, "headers", NULL, 0);
            const char *host_hdr = jn_headers ?
                kw_get_str(gobj, jn_headers, "HOST", "127.0.0.1", 0) : "127.0.0.1";

            char issuer_url[512];
            char token_url[512];
            char logout_url[512];
            snprintf(issuer_url, sizeof(issuer_url),
                "http://%s/realms/test", host_hdr);
            snprintf(token_url, sizeof(token_url),
                "http://%s/realms/test/protocol/openid-connect/token", host_hdr);
            snprintf(logout_url, sizeof(logout_url),
                "http://%s/realms/test/protocol/openid-connect/logout", host_hdr);

            jn_body = json_pack("{s:s, s:s, s:s}",
                "issuer",               issuer_url,
                "token_endpoint",       token_url,
                "end_session_endpoint", logout_url
            );
        }
        /*
         *  Send inline (not via mk_send_json) so the discovery exchange
         *  does NOT increment st_responses_sent — every test's mock-KC
         *  assertions count token+logout responses only; discovery has
         *  its own st_discovery_requests counter.
         */
        json_t *kw_resp = json_pack("{s:s, s:s, s:o}",
            "code",     "200 OK",
            "headers",  "",
            "body",     jn_body
        );
        gobj_send_event(src, EV_SEND_MESSAGE, kw_resp, gobj);
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj,
                "mock-kc → %s: discovery 200", gobj_short_name(src)
            );
        }

    } else if(strstr(url, "/token")) {
        priv->st_token_requests++;
        save_form(gobj, kw);

        if(!empty_string(gobj_read_str_attr(gobj, "raw_answer"))) {
            mk_send_raw_answer(gobj, src, gobj_read_str_attr(gobj, "raw_answer"));
            KW_DECREF(kw)
            return 0;
        }

        if(gobj_read_bool_attr(gobj, "drop_token_request")) {
            gobj_write_bool_attr(gobj, "drop_token_request", FALSE);
            if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
                gobj_trace_msg(gobj, "mock-kc ← %s: /token dropped, no answer",
                    gobj_short_name(src));
            }
            gobj_send_event(gobj_bottom_gobj(src), EV_DROP, 0, gobj);
            KW_DECREF(kw)
            return 0;
        }

        int status            = (int)gobj_read_integer_attr(gobj, "return_status");
        const char *status_tx = (status == 200) ? "200 OK"
                              : (status == 400) ? "400 Bad Request"
                              : (status == 401) ? "401 Unauthorized"
                              : (status == 429) ? "429 Too Many Requests"
                              : (status == 500) ? "500 Internal Server Error"
                                                : "200 OK";

        const char *raw_content_type = gobj_read_str_attr(gobj, "raw_content_type");
        if(!empty_string(raw_content_type)) {
            mk_send_raw(gobj, src, raw_content_type, gobj_read_str_attr(gobj, "raw_body"));
            KW_DECREF(kw)
            return 0;
        }

        json_t *jn_body;
        if(status == 200) {
            jn_body = build_token_envelope(gobj);
        } else {
            jn_body = json_pack("{s:s, s:s}",
                "error",              "invalid_grant",
                "error_description",  "mock keycloak injected error"
            );
        }
        respond_or_defer(gobj, src, status, status_tx, jn_body);

    } else if(strstr(url, "/logout")) {
        priv->st_logout_requests++;
        save_form(gobj, kw);
        if(!empty_string(gobj_read_str_attr(gobj, "raw_answer"))) {
            mk_send_raw_answer(gobj, src, gobj_read_str_attr(gobj, "raw_answer"));
            KW_DECREF(kw)
            return 0;
        }
        json_t *jn_empty = json_object();
        respond_or_defer(gobj, src, 204, "204 No Content", jn_empty);

    } else {
        priv->st_unknown_requests++;
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_PROTOCOL,
            "msg",      "%s", "mock-kc unknown URL",
            "url",      "%s", url,
            NULL
        );
        json_t *jn_err = json_pack("{s:s}", "error", "not_found");
        respond_or_defer(gobj, src, 404, "404 Not Found", jn_err);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Latency timer fired: flush the deferred response to the stashed src.
 *
 *  By the time we get here, the client connection may have been closed
 *  (e.g. the test cancelled it mid-flight) and ac_on_close already
 *  cleared the pending slot.  In that case the slot is empty and we
 *  silently exit — nothing to send.
 ***************************************************************************/
PRIVATE int ac_timer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->pending_body || !priv->pending_src) {
        /*
         *  The pending slot was cleared while the timer was in flight
         *  (e.g. by ac_on_close).  Nothing to do.
         */
        JSON_DECREF(kw)
        return 0;
    }

    hgobj   pending_src  = priv->pending_src;
    int     status       = priv->pending_status;
    json_t *jn_body      = priv->pending_body;

    priv->pending_src    = NULL;
    priv->pending_body   = NULL;

    mk_send_json(gobj, pending_src, status, priv->pending_status_text, jn_body);
    JSON_DECREF(jn_body)

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Absorb EV_STOPPED from our C_TIMER0 child.
 *
 *  C_TIMER0 publishes EV_STOPPED to its parent whenever its yev_event
 *  cancellation completes (clear_timeout0 / mt_stop / yuno teardown).
 *  Without this entry the FSM dispatcher logs "Event NOT DEFINED in
 *  state" for every cancel.  We don't act on it — the lifetime of the
 *  pending slot is already managed by ac_on_close / ac_timer / mt_stop.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Client TCP connection closed.
 *
 *  If a deferred response was pending against this very src, the timer
 *  would fire into a now-dead gobj and its send would be rejected.  We
 *  pre-empt that: cancel the timer, drop the body, bump the cancelled
 *  counter.  (Conservative match — if pending_src != src we leave the
 *  slot alone because it belongs to a different connection.)
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->pending_src == src && priv->pending_body) {
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj,
                "mock-kc pending response cancelled (client closed): src=%s",
                gobj_short_name(src)
            );
        }
        clear_timeout0(priv->timer);
        JSON_DECREF(priv->pending_body)
        priv->pending_src = NULL;
        priv->st_pending_cancelled++;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Connection opened — ignore.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}




                    /***************************
                     *      FSM
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_stats   = mt_stats,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_MOCK_KEYCLOAK);

/***************************************************************************
 *
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
     *      Events / States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_ON_OPEN,        ac_on_open,     0},
        {EV_ON_MESSAGE,     ac_on_message,  0},
        {EV_ON_CLOSE,       ac_on_close,    0},
        {EV_TIMEOUT,        ac_timer,       0},
        {EV_STOPPED,        ac_stopped,     0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_OPEN,        0},
        {EV_ON_MESSAGE,     0},
        {EV_ON_CLOSE,       0},
        {EV_TIMEOUT,        0},
        {EV_STOPPED,        0},
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
        0,              // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,              // authz_table
        0,              // command_table
        s_user_trace_level,
        0               // gcflag_t
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_mock_keycloak(void)
{
    return create_gclass(C_MOCK_KEYCLOAK);
}
