/***********************************************************************
 *          Configuration of supported urls and keycloak
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

/*
 *  Entries must be the same in backend_urls and keycloak_configs.
 *
 *  HACK: Every WebSocket backend added to backend_urls and every OAuth server
 *  added to keycloak_configs MUST also be reflected in the CSP connect-src
 *  directive in index.html, and vice-versa.  Keeping these two files out of
 *  sync will either break connectivity or leave the CSP wider than intended.
 */

const backend_urls = {
    "localhost": "wss://localhost:1800",
    "treedb.yunetas.com": "wss://treedb.yunetas.com:1800"
};

const keycloak_configs = {
    // Connections to keycloak, in connections to localhost it will use real authentication too.
    "localhost": {
        "realm": "estadodelaire.com",
        "auth-server-url": "https://auth.artgins.com/",
        "ssl-required": "external",
        "resource": "gui_treedb",
        "public-client": true,
        "confidential-port": 0
    },
    "treedb.yunetas.com": {
        "realm": "estadodelaire.com",
        "auth-server-url": "https://auth.artgins.com/",
        "ssl-required": "external",
        "resource": "gui_treedb",
        "public-client": true,
        "confidential-port": 0
    }
};

export {
    backend_urls,
    keycloak_configs,
};
