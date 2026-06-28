/***********************************************************************
 *          es.js
 *
 *          Spanish translations.
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
const es = {
    name: "Español",

    translation: {
        /* diálogos */
        "yes":               "Sí",
        "no":                "No",
        "ok":                "Aceptar",
        "cancel":            "Cancelar",
        "accept":            "Aceptar",
        "close":             "Cerrar",
        "save":              "Guardar",
        "delete":            "Eliminar",
        "edit":              "Editar",
        "add":               "Añadir",
        "new":               "Nuevo",
        "refresh":           "Recargar",
        "search":            "Buscar",
        "select":            "Seleccionar",
        "are you sure":      "¿Está seguro?",
        "this field is required": "Campo requerido",

        /* mensajes */
        "error":             "Error",
        "warning":           "Atención",
        "info":              "Información",

        /* autenticación */
        "login":             "Iniciar sesión",
        "logout":            "Cerrar sesión",
        "user":              "Usuario",
        "email":             "Correo electrónico",
        "password":          "Contraseña",

        /* dominio consola */
        "console":           "Consola",
        "agent cli":         "CLI del agente",
        "command":           "Comando",
        "execute":           "Ejecutar",
        "clear":             "Limpiar",
        "response":          "Respuesta",
        "treedb":            "TreeDB",
        "pick a treedb":     "Selecciona un treedb para abrir",
        "stats":             "Estadísticas",
        "settings":          "Configuración",
        "agents":            "Agentes",
        "authentication":    "Autenticación",
        "connected":         "Conectado",
        "disconnected":      "Desconectado",
        "connecting":        "Conectando",
        "no active agent":   "Sin agente activo — añade uno en Configuración",
        "not connected to an agent": "Sin conexión con un agente",
        "cannot connect":    "No se puede conectar",
        "authentication required": "Autenticación requerida",
        "identity card refused": "Tarjeta de identidad rechazada",

        /* configuración · agentes */
        "agents subtitle":   "Endpoints de agentes, guardados en este navegador",
        "no agents configured": "Aún no hay agentes configurados",
        "set active":        "Activar",
        "add agent":         "Añadir agente",
        "edit agent":        "Editar agente",
        "label":             "Etiqueta",
        "endpoint url":      "URL del endpoint",
        "remote role":       "Rol remoto",
        "remote service":    "Servicio remoto",
        "remote name":       "Nombre remoto",
        "label and url are required": "La etiqueta y la URL son obligatorias",
        "url must start with ws or wss": "La URL debe empezar por ws:// o wss://",
        "an agent with this label already exists": "Ya existe un agente con esa etiqueta",

        /* configuración · autenticación */
        "auth subtitle":     "OIDC / Keycloak, guardado en este navegador",
        "provider":          "Proveedor",
        "auth url":          "URL de auth",
        "realm":             "Realm",
        "client id":         "Client ID",
        "session":           "Sesión",
        "username":          "Usuario",
        "logged in as":      "Conectado como",
        "signing in":        "Conectando",
        "logged out":        "Sesión cerrada",
        "username and password are required": "Usuario y contraseña son obligatorios",
        "auth config saved": "Configuración de autenticación guardada",
        "login failed":      "Error de inicio de sesión",

        /* preferencias */
        "select language":   "Seleccionar idioma",
        "select theme":      "Seleccionar tema",
        "dark theme":        "Tema oscuro",
        "light theme":       "Tema claro",
        "system theme":      "Tema del sistema",

        /* mantener al final — insertar nuevas claves antes */
        "_xxx":              "última clave — insertar nuevas antes"
    }
};

export {es};
