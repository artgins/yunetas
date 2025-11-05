/***********************************************************************
 *          Configuration of supported urls and keycloak
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

/*
 *  Entries must be the same in backend_urls and keycloak_configs
 */

const backend_urls = {
    "localhost": "wss://localhost:1800",
    "treedb.yunetas.com": "wss://treedb.yunetas.com:1800"
};

const keycloak_configs = {
    "localhost": {
            "realm": "yunovatios.es",
            "auth-server-url": "https://auth.artgins.com/",
            "ssl-required": "external",
            "resource": "gui_treedb",
            "public-client": true,
            "confidential-port": 0
    },
    "treedb.yunetas.com": {
            "realm": "yunovatios.es",
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
