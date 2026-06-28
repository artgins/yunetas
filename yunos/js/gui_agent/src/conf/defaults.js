/***********************************************************************
 *          defaults.js
 *
 *          NON-private default templates for the connection / auth
 *          config. The real values are entered by the user in the
 *          Settings views and persisted in the browser localStorage
 *          (SDF_PERSIST attrs) — NOTHING private is committed here.
 *
 *          Unlike gui_treedb's conf/backend_config.js (which hardcodes
 *          real endpoints into the bundle), this file only provides
 *          empty shapes and example placeholders so the Settings form
 *          knows the field set.
 *
 *          IMPORTANT: any agent URL the user adds must match one of the
 *          origins listed in config.json "csp_connect_src" — the CSP is
 *          a build-time security boundary the browser enforces.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

/*
 *  Shape of one agent endpoint entry (stored in the C_AGENT_CONFIG
 *  `agents` attr; one is marked active via `active_agent`). The
 *  transport (plain ws / TLS wss) is derived from the url scheme.
 */
const AGENT_TEMPLATE = {
    label:      "",                 // human-friendly name shown in the picker
    url:        "",                 // "wss://host:1993" (OAuth2) | "ws://host:1991" (plain, local)
    yuno_role:  "yuneta_agent",     // remote yuno role (agent default)
    yuno_name:  "",                 // remote yuno name (optional)
    service:    "__default_service__" // remote service that handles commands
};

/*
 *  Default (empty) auth config. Filled by the user in Settings · Auth.
 */
const AUTH_TEMPLATE = {
    auth_url:   "",                 // OIDC/Keycloak base, e.g. "https://auth.artgins.com"
    realm:      "",
    client_id:  ""
};

export {AGENT_TEMPLATE, AUTH_TEMPLATE};
