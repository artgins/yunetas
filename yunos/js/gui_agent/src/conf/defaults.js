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
 *  Shape of one agent endpoint entry (stored in the `agents` attr).
 */
const AGENT_TEMPLATE = {
    label:      "",                 // human-friendly name shown in the picker
    url:        "",                 // e.g. "wss://app.wattyzer.com:1993" (plain: ":1991")
    yuno_role:  "yuneta_agent",     // remote yuno role (agent default)
    yuno_name:  "",                 // remote yuno name (optional)
    service:    "__default_service__", // remote service that handles commands
    transport:  "wss"               // "wss" (OAuth2) | "ws" (plain, local only)
};

/*
 *  Example entries to seed an EMPTY config — the user edits/replaces
 *  these in Settings. They carry no secrets (no jwt, no password).
 */
const EXAMPLE_AGENTS = [
    {
        label:      "Local agent (plain)",
        url:        "wss://localhost:1991",
        yuno_role:  "yuneta_agent",
        yuno_name:  "",
        service:    "__default_service__",
        transport:  "ws"
    }
];

/*
 *  Default (empty) auth config. Filled by the user in Settings · Auth.
 */
const AUTH_TEMPLATE = {
    auth_url:   "",                 // OIDC/Keycloak base, e.g. "https://auth.artgins.com"
    realm:      "",
    client_id:  ""
};

export {AGENT_TEMPLATE, EXAMPLE_AGENTS, AUTH_TEMPLATE};
