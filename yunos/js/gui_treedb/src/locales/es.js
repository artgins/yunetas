/***********************************************************************
 *          es.js
 *
 *          Traducciones al español.
 *
 *          Convention (all locale files share these rules):
 *            1. Keys are lower-case ASCII English.
 *            2. Values are sentence-case in their target language —
 *               a missing translation falls through to the lower-case
 *               key, making the gap visible to the user at a glance.
 *            3. Every locale file must carry the *same* key set; see
 *               scripts/validate-locales.mjs.
 *
 *          Two key shapes coexist on purpose:
 *            - Free-text UI keys use spaces ("connection to backend
 *              refused").
 *            - Backend protocol IDs (auth_bff error_code values, see
 *              kernel/c/root-linux/src/c_auth_bff.c) keep their
 *              snake_case form so they match the wire format
 *              one-to-one.  Both forms are still ASCII lower-case.
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
const es = {
    name: "Español",

    translation: {
        /* etiquetas de UI (llamadas vía t() en c_yuneta_gui.js) */
        "cause":                              "Motivo",
        "user":                               "Usuario",
        "url":                                "URL",
        "remote service":                     "Servicio remoto",
        "connection to backend refused":      "Conexión al backend rechazada",
        "no yuneta backend url available":    "No hay URL de Yuneta backend disponible",
        "no registered url for remote service": "No hay URL registrada para este servicio remoto",

        /* códigos auth_bff — IDs de protocolo, ver c_auth_bff.c */
        "invalid_credentials":                "Usuario o contraseña incorrectos",
        "session_expired":                    "Su sesión ha caducado. Por favor, inicie sesión de nuevo.",
        "account_disabled":                   "Cuenta deshabilitada o no configurada. Contacte con el administrador.",
        "auth_rate_limited":                  "Demasiados intentos. Espere un momento e inténtelo de nuevo.",
        "auth_service_unavailable":           "Servicio de autenticación no disponible. Inténtelo más tarde.",
        "auth_config_error":                  "No se pudo iniciar sesión. Inténtelo más tarde o contacte con el administrador.",
        "auth_unexpected_error":              "No se pudo iniciar sesión. Si el problema persiste, contacte con el administrador.",
        "auth_empty_response":                "No se pudo iniciar sesión. Inténtelo más tarde.",
        "auth_timeout":                       "No se pudo iniciar sesión. Inténtelo más tarde.",
        "network_error":                      "Error de red. Compruebe la conexión e inténtelo de nuevo.",
        "refresh_denied":                     "Su sesión ha caducado. Por favor, inicie sesión de nuevo.",
        "server_busy":                        "El servidor está ocupado. Inténtelo en un momento.",

        /* mantener al final — insertar nuevas claves antes */
        "_xxx":                               "last key — insert new ones above"
    }
};

export {es};
