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
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  Build the synthesised token envelope (all KW_REQUIRED fields that
 *  c_task_authenticate::result_get_token reads must be present).
 ***************************************************************************/
PRIVATE json_t *build_token_envelope(void)
{
    return json_pack(
        "{s:s, s:s, s:i, s:i, s:s, s:s, s:s}",
        "access_token",         "mock-access-token",
        "refresh_token",        "mock-refresh-token",
        "expires_in",           300,
        "refresh_expires_in",   1800,
        "token_type",           "Bearer",
        "session_state",        "mock-session-state",
        "scope",                "openid profile email"
    );
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
    const char *url = kw_get_str(gobj, kw, "request_url", "", 0);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "mock-idp ← %s: method=%d url=%s",
            gobj_short_name(src), method, url
        );
    }

    if(strstr(url, "/.well-known/openid-configuration")) {
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

    } else if(strstr(url, "/token")) {
        int status            = (int)gobj_read_integer_attr(gobj, "return_status_token");
        const char *status_tx = (status == 200) ? "200 OK"
                              : (status == 400) ? "400 Bad Request"
                              : (status == 401) ? "401 Unauthorized"
                              : (status == 500) ? "500 Internal Server Error"
                                                : "200 OK";

        json_t *override = gobj_read_json_attr(gobj, "override_token_body");
        json_t *jn_body;
        if(override && json_object_size(override) > 0) {
            jn_body = json_deep_copy(override);
        } else if(status == 200) {
            jn_body = build_token_envelope();
        } else {
            jn_body = json_pack("{s:s, s:s}",
                "error",              "invalid_grant",
                "error_description",  "mock idp injected error"
            );
        }
        send_resp(gobj, src, status, status_tx, jn_body);

    } else if(strstr(url, "/logout")) {
        send_resp(gobj, src, 204, "204 No Content", json_object());

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
