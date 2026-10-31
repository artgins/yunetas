/****************************************************************************
 *          C_MOCK_IDP.C
 *
 *          Test-only mock of an OIDC IdP token endpoint.  See the
 *          companion .h for the public surface and the per-attr protocol.
 *
 *          The gclass sits under a C_CHANNEL in the test yuno's __idp_side__
 *          IOGate, with a C_PROT_HTTP_SR child underneath.  The http_sr
 *          automatically subscribes to its parent, so we receive every
 *          parsed request as EV_ON_MESSAGE and reply via EV_SEND_MESSAGE.
 *
 *          c_task_authenticate parses the access_token field as an opaque
 *          string and does NOT validate JWT structure or signature today,
 *          so the mock returns plain placeholder strings — no JWT signing
 *          dependency.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include <yunetas.h>

#include "c_mock_idp.h"

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name------------------------flag----default---description*/
SDATA (DTP_INTEGER,     "return_status_token",      SDF_RD, "200",    "HTTP status returned by /token"),
SDATA (DTP_INTEGER,     "return_status_discovery",  SDF_RD, "200",    "HTTP status returned by /.well-known/openid-configuration"),
SDATA (DTP_JSON,        "override_token_body",      SDF_RD, "{}",     "If non-empty, sent verbatim instead of the synthesised /token envelope"),
SDATA (DTP_JSON,        "override_discovery_body",  SDF_RD, "{}",     "If non-empty, sent verbatim instead of the synthesised /.well-known body (e.g. to omit endpoints)"),
SDATA (DTP_INTEGER,     "drop_token_requests",      SDF_RD, "0",      "So many /token requests are not answered: their connection is dropped (an IdP that restarts)"),
SDATA (DTP_INTEGER,     "drop_logout_requests",     SDF_RD, "0",      "So many /logout requests are not answered: their connection is dropped"),
SDATA (DTP_INTEGER,     "drop_discovery_requests",  SDF_RD, "0",      "So many /.well-known requests are not answered: their connection is dropped"),
SDATA (DTP_INTEGER,     "expected_logouts",         SDF_RD, "-1",     "If >= 0, the mock logs an ERROR when it stops with another count of /logout requests"),
SDATA (DTP_INTEGER,     "expected_bearer_len",      SDF_RD, "0",      "If > 0, a /logout whose Authorization header has not this length logs an ERROR"),
SDATA (DTP_INTEGER,     "access_token_len",         SDF_RD, "0",      "If > 0, the access_token of the /token envelope has this length"),
SDATA (DTP_INTEGER,     "logouts_seen",             SDF_RD, "0",      "The /logout requests that have arrived"),
SDATA (DTP_INTEGER,     "admin_status",             SDF_RD, "200",    "HTTP status of an /admin/ request"),
SDATA (DTP_INTEGER,     "drop_admin_requests",      SDF_RD, "0",      "So many /admin/ requests are not answered: their connection is dropped"),
SDATA (DTP_INTEGER,     "admin_seen",               SDF_RD, "0",      "The /admin/ requests that have arrived; a test clears it"),
SDATA (DTP_STRING,      "last_admin_url",           SDF_RD, "",       "The url of the last /admin/ request"),
SDATA (DTP_INTEGER,     "token_seen",               SDF_RD, "0",      "The /token requests that have arrived; a test clears it"),
SDATA (DTP_STRING,      "raw_token_answer",         SDF_RD, "",       "If non-empty, /token is answered with these bytes as they are, status line and headers included; \"@bighead\": a 200 with a header of 70 KB"),
SDATA (DTP_DICT,        "expected_form",            SDF_RD, "{}",     "If non-empty, each key must be in the /token form ONCE, with this value once decoded; else the mock logs an ERROR and answers 400 invalid_grant"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"messages",    "Trace mock IdP request/response flow"},
    {0, 0}
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int dummy;  /* gclass_create rejects sizeof(PRIVATE_DATA)==0 in some configs */
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
     *  C_PROT_HTTP_SR (our child, plugged in by the yuno tree) auto-
     *  subscribes to its parent in its own mt_create — EV_ON_MESSAGE
     *  arrives naturally without further wiring.
     */
}

/***************************************************************************
 *      Framework Method stop
 *
 *  C_CHANNEL stops its bottom (us) but does not recurse, so the inbound
 *  C_PROT_HTTP_SR + C_TCP stack would otherwise stay RUNNING during yuno
 *  teardown.  Stop the whole bottom chain explicitly.
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    hgobj bottom = gobj_bottom_gobj(gobj);
    if(bottom && gobj_is_running(bottom)) {
        gobj_stop_tree(bottom);
    }

    json_int_t expected = gobj_read_integer_attr(gobj, "expected_logouts");
    json_int_t seen = gobj_read_integer_attr(gobj, "logouts_seen");
    if(expected >= 0 && expected != seen) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "mock-idp: the /logout requests that arrived",
            "expected", "%d", (int)expected,
            "seen",     "%d", (int)seen,
            NULL
        );
    }
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  Build the synthesised token envelope (all KW_REQUIRED fields that
 *  c_task_authenticate::result_get_token reads must be present).
 ***************************************************************************/
PRIVATE json_t *build_token_envelope(hgobj gobj)
{
    char access_token[9000] = "mock-access-token";
    int access_token_len = (int)gobj_read_integer_attr(gobj, "access_token_len");
    if(access_token_len > 0 && access_token_len < (int)sizeof(access_token)) {
        memset(access_token, 'a', (size_t)access_token_len);
        access_token[access_token_len] = '\0';
    }
    return json_pack(
        "{s:s, s:s, s:i, s:i, s:s, s:s, s:s}",
        "access_token",         access_token,
        "refresh_token",        "mock-refresh-token",
        "expires_in",           300,
        "refresh_expires_in",   1800,
        "token_type",           "Bearer",
        "session_state",        "mock-session-state",
        "scope",                "openid profile email"
    );
}

/***************************************************************************
 *  Decode a part of a form, application/x-www-form-urlencoded, into `out`
 ***************************************************************************/
PRIVATE void decode_form_text(const char *text, size_t len, char *out)
{
    size_t o = 0;
    for(size_t i = 0; i < len; i++) {
        if(text[i] == '+') {
            out[o++] = ' ';
        } else if(text[i] == '%' && i + 2 < len) {
            char hex[3] = {text[i+1], text[i+2], 0};
            out[o++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else {
            out[o++] = text[i];
        }
    }
    out[o] = '\0';
}

/***************************************************************************
 *  The /token form against `expected_form`: TRUE when each key of it is
 *  in the form once, with its value. A mismatch is logged.
 ***************************************************************************/
PRIVATE BOOL form_as_expected(hgobj gobj, json_t *kw)
{
    json_t *expected = gobj_read_json_attr(gobj, "expected_form");
    if(json_object_size(expected) == 0) {
        return TRUE;
    }
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    char *form = gbuf?
        gbmem_strndup(gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf)) :
        gbmem_strdup("");
    if(!form) {
        return FALSE;  // Error already logged
    }

    json_t *jn_form = json_object();
    const char *p = form;
    while(*p) {
        size_t field_len = strcspn(p, "&");
        const char *eq = memchr(p, '=', field_len);
        size_t key_len = eq? (size_t)(eq - p) : field_len;
        char *key = gbmem_malloc(key_len + 1);
        char *value = gbmem_malloc(field_len + 1);
        if(key && value) {
            decode_form_text(p, key_len, key);
            value[0] = '\0';
            if(eq) {
                decode_form_text(eq + 1, field_len - key_len - 1, value);
            }
            json_t *jn_values = json_object_get(jn_form, key);
            if(!jn_values) {
                jn_values = json_array();
                json_object_set_new(jn_form, key, jn_values);
            }
            json_array_append_new(jn_values, json_string(value));
        }
        GBMEM_FREE(key)
        GBMEM_FREE(value)
        p += field_len;
        if(*p == '&') {
            p++;
        }
    }

    BOOL ok = TRUE;
    const char *key;
    json_t *jn_value;
    json_object_foreach(expected, key, jn_value) {
        json_t *jn_values = json_object_get(jn_form, key);
        if(json_array_size(jn_values) != 1 ||
                strcmp(json_string_value(json_array_get(jn_values, 0)),
                    json_string_value(jn_value)? json_string_value(jn_value) : "") != 0) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset",   "%s", MSGSET_APP,
                "msg",      "%s", "mock-idp: the /token form has not a key once, with its value",
                "key",      "%s", key,
                "form",     "%.300s", form,
                NULL
            );
            ok = FALSE;
        }
    }
    JSON_DECREF(jn_form)
    GBMEM_FREE(form)
    return ok;
}

/***************************************************************************
 *  The bytes of `raw_token_answer` written as they are, straight to the
 *  transport below the http_sr child that delivered the request
 ***************************************************************************/
PRIVATE void send_raw_answer(hgobj gobj, hgobj http_sr, const char *raw)
{
    gbuffer_t *gbuf;
    if(strcmp(raw, "@bighead") == 0) {
        const char *start = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nX-Big: ";
        const char *end = "\r\nContent-Length: 2\r\n\r\n{}";
        size_t big = 70*1024;
        gbuf = gbuffer_create(strlen(start) + big + strlen(end), strlen(start) + big + strlen(end));
        if(!gbuf) {
            return;  // Error already logged
        }
        gbuffer_append(gbuf, (void *)start, strlen(start));
        for(size_t i = 0; i < big; i++) {
            gbuffer_append_char(gbuf, 'h');
        }
        gbuffer_append(gbuf, (void *)end, strlen(end));
    } else {
        size_t len = strlen(raw);
        gbuf = gbuffer_create(len, len);
        if(!gbuf) {
            return;  // Error already logged
        }
        gbuffer_append(gbuf, (void *)raw, len);
    }
    json_t *kw_tx = json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf);
    gobj_send_event(gobj_bottom_gobj(http_sr), EV_TX_DATA, kw_tx, gobj);
}

/***************************************************************************
 *  Send a JSON response back to the http_sr child that delivered `src`.
 ***************************************************************************/
PRIVATE void send_resp(hgobj gobj, hgobj http_sr, int status,
    const char *status_text, json_t *jn_body)
{
    json_t *kw_resp = json_pack("{s:s, s:s, s:o}",
        "code",     status_text,
        "headers",  "",
        "body",     jn_body
    );
    gobj_send_event(http_sr, EV_SEND_MESSAGE, kw_resp, gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "mock-idp → %s: status=%d", gobj_short_name(http_sr), status
        );
    }
    (void)status;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  EV_ON_MESSAGE: a parsed HTTP request arrived from C_PROT_HTTP_SR.
 *  Route by URL substring (the task posts to URLs ending in
 *  /.well-known/openid-configuration, /token or /logout).
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    int method = (int)kw_get_int(gobj, kw, "request_method", 0, 0);
    const char *url = kw_get_str(gobj, kw, "url", "", 0);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "mock-idp ← %s: method=%d url=%s",
            gobj_short_name(src), method, url
        );
    }

    if(strstr(url, "/.well-known/openid-configuration") &&
            gobj_read_integer_attr(gobj, "drop_discovery_requests") > 0) {
        gobj_write_integer_attr(gobj, "drop_discovery_requests",
            gobj_read_integer_attr(gobj, "drop_discovery_requests") - 1);
        gobj_send_event(gobj_bottom_gobj(src), EV_DROP, 0, gobj);

    } else if(strstr(url, "/.well-known/openid-configuration")) {
        int status            = (int)gobj_read_integer_attr(gobj, "return_status_discovery");
        const char *status_tx = (status == 200) ? "200 OK" : "500 Internal Server Error";

        json_t *override = gobj_read_json_attr(gobj, "override_discovery_body");
        json_t *jn_body;
        if(override && json_object_size(override) > 0) {
            /* Tests use this to return a discovery body that omits one
             * of the required endpoints, exercising the
             * result_save_discovery failure path. */
            jn_body = json_deep_copy(override);
        } else {
            json_t *jn_headers = kw_get_dict(gobj, kw, "headers", NULL, 0);
            const char *host_hdr = jn_headers ?
                kw_get_str(gobj, jn_headers, "HOST", "127.0.0.1", 0) : "127.0.0.1";
            char issuer[256], token_url[256], logout_url[256];
            snprintf(issuer,     sizeof(issuer),
                "http://%s/realms/test", host_hdr);
            snprintf(token_url,  sizeof(token_url),
                "http://%s/realms/test/protocol/openid-connect/token", host_hdr);
            snprintf(logout_url, sizeof(logout_url),
                "http://%s/realms/test/protocol/openid-connect/logout", host_hdr);
            jn_body = json_pack("{s:s, s:s, s:s}",
                "issuer",               issuer,
                "token_endpoint",       token_url,
                "end_session_endpoint", logout_url
            );
        }
        send_resp(gobj, src, status, status_tx, jn_body);

    } else if(strstr(url, "/admin/")) {
        gobj_write_integer_attr(gobj, "admin_seen",
            gobj_read_integer_attr(gobj, "admin_seen") + 1);
        gobj_write_str_attr(gobj, "last_admin_url", url);
        if(gobj_read_integer_attr(gobj, "drop_admin_requests") > 0) {
            gobj_write_integer_attr(gobj, "drop_admin_requests",
                gobj_read_integer_attr(gobj, "drop_admin_requests") - 1);
            gobj_send_event(gobj_bottom_gobj(src), EV_DROP, 0, gobj);
        } else if(gobj_read_integer_attr(gobj, "admin_status") == 200) {
            send_resp(gobj, src, 200, "200 OK", json_pack("{s:s}", "id", "abc"));
        } else {
            send_resp(gobj, src, 204, "204 No Content", json_object());
        }

    } else if(strstr(url, "/token") && gobj_read_integer_attr(gobj, "drop_token_requests") > 0) {
        gobj_write_integer_attr(gobj, "token_seen",
            gobj_read_integer_attr(gobj, "token_seen") + 1);
        gobj_write_integer_attr(gobj, "drop_token_requests",
            gobj_read_integer_attr(gobj, "drop_token_requests") - 1);
        gobj_send_event(gobj_bottom_gobj(src), EV_DROP, 0, gobj);

    } else if(strstr(url, "/token") && !empty_string(gobj_read_str_attr(gobj, "raw_token_answer"))) {
        gobj_write_integer_attr(gobj, "token_seen",
            gobj_read_integer_attr(gobj, "token_seen") + 1);
        send_raw_answer(gobj, src, gobj_read_str_attr(gobj, "raw_token_answer"));

    } else if(strstr(url, "/token")) {
        gobj_write_integer_attr(gobj, "token_seen",
            gobj_read_integer_attr(gobj, "token_seen") + 1);
        int status            = (int)gobj_read_integer_attr(gobj, "return_status_token");
        const char *status_tx = (status == 200) ? "200 OK"
                              : (status == 400) ? "400 Bad Request"
                              : (status == 401) ? "401 Unauthorized"
                              : (status == 500) ? "500 Internal Server Error"
                                                : "200 OK";

        BOOL form_ok = form_as_expected(gobj, kw);
        if(!form_ok) {
            status = 400;
            status_tx = "400 Bad Request";
        }

        json_t *override = gobj_read_json_attr(gobj, "override_token_body");
        json_t *jn_body;
        if(form_ok && override && json_object_size(override) > 0) {
            jn_body = json_deep_copy(override);
        } else if(status == 200) {
            jn_body = build_token_envelope(gobj);
        } else {
            jn_body = json_pack("{s:s, s:s}",
                "error",              "invalid_grant",
                "error_description",  "mock idp injected error"
            );
        }
        send_resp(gobj, src, status, status_tx, jn_body);

    } else if(strstr(url, "/logout")) {
        gobj_write_integer_attr(gobj, "logouts_seen",
            gobj_read_integer_attr(gobj, "logouts_seen") + 1);
        json_int_t bearer_len = gobj_read_integer_attr(gobj, "expected_bearer_len");
        json_t *jn_headers = kw_get_dict(gobj, kw, "headers", NULL, 0);
        const char *authorization = kw_get_str(gobj, jn_headers, "AUTHORIZATION", "", 0);
        if(bearer_len > 0 && (json_int_t)strlen(authorization) != bearer_len) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset",   "%s", MSGSET_APP,
                "msg",      "%s", "mock-idp: the Authorization of the /logout is cut",
                "expected", "%d", (int)bearer_len,
                "got",      "%d", (int)strlen(authorization),
                NULL
            );
        }
        if(gobj_read_integer_attr(gobj, "drop_logout_requests") > 0) {
            gobj_write_integer_attr(gobj, "drop_logout_requests",
                gobj_read_integer_attr(gobj, "drop_logout_requests") - 1);
            gobj_send_event(gobj_bottom_gobj(src), EV_DROP, 0, gobj);
        } else {
            send_resp(gobj, src, 204, "204 No Content", json_object());
        }

    } else {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_PROTOCOL,
            "msg",      "%s", "mock-idp unknown URL",
            "url",      "%s", url,
            NULL
        );
        json_t *jn_err = json_pack("{s:s}", "error", "not_found");
        send_resp(gobj, src, 404, "404 Not Found", jn_err);
    }

    KW_DECREF(kw)
    return 0;
}




                    /***************************
                     *      FSM
                     ***************************/




PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_stop   = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_MOCK_IDP);

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

    ev_action_t st_idle[] = {
        {EV_ON_MESSAGE,     ac_on_message,  0},
        {EV_ON_OPEN,        0,              0},
        {EV_ON_CLOSE,       0,              0},
        {EV_STOPPED,        0,              0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_MESSAGE,     0},
        {EV_ON_OPEN,        0},
        {EV_ON_CLOSE,       0},
        {EV_STOPPED,        0},
        {0, 0}
    };

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
PUBLIC int register_c_mock_idp(void)
{
    return create_gclass(C_MOCK_IDP);
}
