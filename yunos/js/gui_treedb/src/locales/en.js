/***********************************************************************
 *          en.js
 *
 *          English translations.
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
const en = {
    name: "English",

    translation: {
        /* free-text UI labels (called via t() in c_yuneta_gui.js) */
        "cause":                              "Cause",
        "user":                               "User",
        "url":                                "URL",
        "remote service":                     "Remote service",
        "connection to backend refused":      "Connection to backend refused",
        "no yuneta backend url available":    "No Yuneta backend URL available",
        "no registered url for remote service": "No registered URL for this remote service",

        /* auth_bff error codes — wire IDs, see c_auth_bff.c */
        "invalid_credentials":                "Invalid username or password",
        "session_expired":                    "Your session has expired. Please log in again.",
        "account_disabled":                   "Account disabled or not fully configured. Please contact the administrator.",
        "auth_rate_limited":                  "Too many login attempts. Please wait a moment and try again.",
        "auth_service_unavailable":           "Authentication service unavailable. Please try again later.",
        "auth_config_error":                  "Could not log in. Please try again later or contact the administrator.",
        "auth_unexpected_error":              "Could not log in. If the problem persists, please contact the administrator.",
        "auth_empty_response":                "Could not log in. Please try again later.",
        "auth_timeout":                       "Could not log in. Please try again later.",
        "network_error":                      "Network error. Please check your connection and try again.",
        "refresh_denied":                     "Your session has expired. Please log in again.",
        "server_busy":                        "The server is busy. Please try again in a moment.",

        /* keep this last so adding new keys above never hits the comma trap */
        "_xxx":                               "last key — insert new ones above"
    }
};

export {en};
