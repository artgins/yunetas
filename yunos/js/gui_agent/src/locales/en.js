/***********************************************************************
 *          en.js
 *
 *          English translations.
 *
 *          Convention (all locale files share these rules):
 *            1. Keys are lower-case ASCII English.
 *            2. Values are sentence-case in their target language — a
 *               missing translation falls through to the lower-case key,
 *               making the gap visible to the user at a glance.
 *            3. Every locale file must carry the *same* key set; see
 *               scripts/validate-locales.mjs.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
const en = {
    name: "English",

    translation: {
        /* dialogs */
        "yes":               "Yes",
        "no":                "No",
        "ok":                "OK",
        "cancel":            "Cancel",
        "accept":            "Accept",
        "close":             "Close",
        "save":              "Save",
        "delete":            "Delete",
        "edit":              "Edit",
        "add":               "Add",
        "new":               "New",
        "refresh":           "Refresh",
        "search":            "Search",
        "select":            "Select",
        "are you sure":      "Are you sure?",
        "this field is required": "This field is required",

        /* messages */
        "error":             "Error",
        "warning":           "Warning",
        "info":              "Information",

        /* auth */
        "login":             "Sign In",
        "logout":            "Sign Out",
        "user":              "User",
        "email":             "Email",
        "password":          "Password",

        /* console domain */
        "console":           "Console",
        "agent cli":         "Agent CLI",
        "command":           "Command",
        "execute":           "Execute",
        "clear":             "Clear",
        "response":          "Response",
        "treedb":            "TreeDB",
        "stats":             "Stats",
        "settings":          "Settings",
        "agents":            "Agents",
        "authentication":    "Authentication",
        "connected":         "Connected",
        "disconnected":      "Disconnected",
        "connecting":        "Connecting",
        "no active agent":   "No active agent — add one in Settings",
        "not connected to an agent": "Not connected to an agent",
        "cannot connect":    "Cannot connect",
        "authentication required": "Authentication required",
        "identity card refused": "Identity card refused",

        /* settings · agents */
        "agents subtitle":   "Agent endpoints, stored in this browser",
        "no agents configured": "No agents configured yet",
        "set active":        "Set active",
        "add agent":         "Add agent",
        "edit agent":        "Edit agent",
        "label":             "Label",
        "endpoint url":      "Endpoint URL",
        "remote role":       "Remote role",
        "remote service":    "Remote service",
        "remote name":       "Remote name",
        "label and url are required": "Label and URL are required",
        "url must start with ws or wss": "URL must start with ws:// or wss://",
        "an agent with this label already exists": "An agent with this label already exists",

        /* prefs */
        "select language":   "Select Language",
        "select theme":      "Select Theme",
        "dark theme":        "Dark theme",
        "light theme":       "Light theme",
        "system theme":      "System theme",

        /* keep this last so adding new keys above never hits the comma trap */
        "_xxx":              "last key — insert new ones above"
    }
};

export {en};
