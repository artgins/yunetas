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
    "localhost": "wss://localhost:1600",
    "hidrauliaconnect.es": "wss://hidrauliaconnect.es:1600",
    "hidrauliaconnect.ovh": "wss://hidrauliaconnect.ovh:1600"
};

const keycloak_configs = {
    "localhost":
        {
            "realm": "estadodelaire.com",
            "auth-server-url": "https://auth.artgins.com/",
            "ssl-required": "external",
            "resource": "hidrauliaconnect.es",
            "public-client": true,
            "confidential-port": 0
        },
    "hidrauliaconnect.es":
        {
            "realm": "estadodelaire.com",
            "auth-server-url": "https://auth.artgins.com/",
            "ssl-required": "external",
            "resource": "hidrauliaconnect.es",
            "public-client": true,
            "confidential-port": 0
        },
    "hidrauliaconnect.ovh":
        {
            "realm": "estadodelaire.com",
            "auth-server-url": "https://auth.artgins.com/",
            "ssl-required": "external",
            "resource": "hidrauliaconnect.es",
            "public-client": true,
            "confidential-port": 0
        },
};

export {
    backend_urls,
    keycloak_configs,
};
