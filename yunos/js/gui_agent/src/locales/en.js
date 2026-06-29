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
        "graph":             "Graph",
        "table":             "Table",
        "live":              "Live",
        "pick a treedb":     "Select a treedb to open",
        "nodes":             "Nodes",
        "nodes subtitle":    "Nodes connected to the control center",
        "no nodes":          "No nodes connected",
        "select a node":     "Select a node",
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

        /* settings · authentication */
        "auth subtitle":     "OIDC / Keycloak, stored in this browser",
        "provider":          "Provider",
        "bff url":           "BFF URL",
        "bff help":          "Auth BFF base URL. Leave empty to use https://<this host>:1806.",
        "auth url":          "Auth URL",
        "realm":             "Realm",
        "client id":         "Client ID",
        "session":           "Session",
        "username":          "Username",
        "logged in as":      "Logged in as",
        "signing in":        "Signing in",
        "logged out":        "Logged out",
        "username and password are required": "Username and password are required",
        "auth config saved": "Authentication settings saved",
        "login failed":      "Login failed",

        /* prefs */
        "select language":   "Select Language",
        "select theme":      "Select Theme",
        "dark theme":        "Dark theme",
        "light theme":       "Light theme",
        "system theme":      "System theme",

        /* account menu · preference / developer / about */
        "preferences":       "Preferences",
        "appearance":        "Appearance",
        "theme":             "Theme",
        "language":          "Language",
        "developer":         "Developer",
        "diagnostics":       "Diagnostics",
        "application":       "Application",
        "version":           "Version",
        "tenant":            "Tenant",
        "plane":             "Plane",
        "host":              "Host",
        "control center":    "Control center",
        "auth bff":          "Auth BFF",
        "active node":       "Active node",
        "none":              "None",
        "about":             "About",
        "agent console":     "Agent Console",
        "about description": "Browser console to operate Yuneta agents through the control center.",
        "documentation":     "Documentation",

        /* keep this last so adding new keys above never hits the comma trap */
        "_xxx":              "last key — insert new ones above"
    }
};

export {en};
