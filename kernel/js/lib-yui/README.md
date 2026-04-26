# lib-yui — Yuneta UI Library

Reusable GUI components for building Yuneta-based web applications. Every
component is a GClass (from [`@yuneta/gobj-js`](https://www.npmjs.com/package/@yuneta/gobj-js))
that plugs into the GObject tree and communicates via events.

## Which app shell to use?

`lib-yui` ships two app-shell stacks. They coexist; pick one per app.

- **New GUIs → `C_YUI_SHELL` + `C_YUI_NAV`** (declarative shell, since
  v7.4.0). JSON-driven layout, routed stages, drawer overlay with
  focus-trap, hot-swap i18n. See [`SHELL.md`](./SHELL.md).
- **Existing GUIs → keep `C_YUI_MAIN` + `C_YUI_ROUTING`.** Both are
  still shipped and supported. Migrating is opt-in, not mandated;
  there is no scheduled removal.

The two stacks **do not share CSS** — do not import
`c_yui_main.css` and `c_yui_shell.css` together. One app picks one
stack.

## Components

- **App shell (declarative)** (`C_YUI_SHELL` + `C_YUI_NAV`) — JSON-driven
  zones, menus, toolbar, drawers, lifecycle. Recommended for new GUIs.
- **App shell (legacy)** (`C_YUI_MAIN`) — layered layout with toolbar,
  modals, and notifications. Still supported for existing GUIs.
- **Window manager** (`C_YUI_WINDOW`) — draggable/resizable floating windows.
- **Tabs** (`C_YUI_TABS`) — tab container for sub-components.
- **Form builder** (`C_YUI_FORM`) — dynamic forms backed by Tabulator and TomSelect.
- **Routing (legacy)** (`C_YUI_ROUTING`) — hash-based menu/content
  routing. Used by `C_YUI_MAIN`-based apps. New apps route through
  `C_YUI_SHELL` instead.
- **Map** (`C_YUI_MAP`) — MapLibre GL map wrapper.
- **Charts** (`C_YUI_UPLOT`) — uPlot chart wrapper.
- **JSON viewer** (`C_YUI_JSON_GRAPH`) — JSON visualization with AntV/G6.
- **TreeDB UI** — topic list, table+form editor, graph manager, and G6-based node graph editor for interacting with Yuneta TreeDB backends.

Extracted from `gui_treedb` to be shared across projects (EstadoDelAire, TreeDB GUI, etc.).

## Install

This project uses [`vite`](https://vite.dev/) as build tool.

Install the latest `node`:

    nvm install --lts

When writing this readme the LTS version was:

    node --version
        v22.17.0

    npm install -g vite

Install dependencies:

    npm install

To build:

    vite build

To publish a new version of @yuneta/lib-yui to [npmjs.com](https://www.npmjs.com/package/@yuneta/lib-yui):

    # 1. Configure your npm token (only once)
    echo "//registry.npmjs.org/:_authToken=<your-token>" > ~/.npmrc

    # 2. Update the version in package.json
    npm version patch   # or minor / major

    # 3. Publish (build runs automatically via prepublishOnly)
    npm publish --access public

To create an npm token, go to [npmjs.com](https://www.npmjs.com) → Account → Access Tokens.

## Update

ONLY one time: to update all js packages, install the module:

    npm install -g npm-check-updates

To download new releases:

    ncu -u

And to install the new versions:

    npm install

---

## Quick Start

### 1. Add dependency

In your project's `package.json`:

```json
{
    "dependencies": {
        "@yuneta/lib-yui": "file:../../../yunetas/kernel/js/lib-yui",
        "@yuneta/gobj-js": "file:../../../yunetas/kernel/js/gobj-js"
    }
}
```

Adjust the relative path to match your directory layout.

### 2. Register GClasses in `main.js`

```js
import {
    register_c_yui_main,
    register_c_yui_window,
    register_c_yui_tabs,
    register_c_yui_form,
    register_c_yui_routing,
    register_c_yui_map,
    register_c_yui_uplot,
    register_c_yui_treedb_topics,
    register_c_yui_treedb_graph,
    inject_svg_icons,
} from "@yuneta/lib-yui";

// Register in main():
register_c_yui_main();
register_c_yui_window();
register_c_yui_tabs();
// ... etc.
```

### 3. Import CSS

```js
import "@yuneta/lib-yui/src/c_yui_main.css";
import "@yuneta/lib-yui/src/c_yui_map.css";
import "@yuneta/lib-yui/src/c_yui_routing.css";
import "@yuneta/lib-yui/src/ytable.css";
import "@yuneta/lib-yui/src/yui_toolbar.css";
import "@yuneta/lib-yui/src/lib_graph.css";
import "@yuneta/lib-yui/src/yui_icons.css";
```

### 4. Configure `C_YUI_MAIN` with your app branding

`c_yui_main.js` does **not** import logos or flags directly. Your app provides
them as GClass attributes:

```js
// In your c_yuneta_gui.js:
import {logo_wide_svg} from "./logos_svg.js";      // your app logo
import {flags_of_world} from "./locales/flags.js";  // your app flags

__yuno__.__yui_main__ = gobj_create_service(
    "__yui_main__",
    "C_YUI_MAIN",
    {
        logo_wide_svg: logo_wide_svg,
        flags_of_world: flags_of_world,
    },
    gobj
);
```

### 5. Vite configuration

```js
import { defineConfig } from "vite";
import { yunetaHtmlPlugin } from "@yuneta/lib-yui/vite-plugin-yuneta-html.js";

export default defineConfig({
    resolve: {
        preserveSymlinks: true,
    },
    plugins: [
        yunetaHtmlPlugin({ defaultTitle: "My App" })
    ]
});
```

> **Note**: `preserveSymlinks: true` is required so that Vite resolves
> `import "@yuneta/gobj-js"` from within yui-lib's source files using the host
> app's `node_modules/`.

## Project Structure

```
lib-yui/
├── index.js                           # Barrel re-exports
├── package.json                       # Peer dependencies
├── vite-plugin-yuneta-html.js         # Shared Vite plugin
├── skeleton/                          # Template files for new projects
│   ├── index.html                     #   HTML template with placeholders
│   └── config.json                    #   Documented config schema
└── src/
    ├── Components ─────────────────────────────────────────────
    │   c_yui_main.js        Main app shell (layers, toolbar, modals)
    │   c_yui_window.js      Draggable/resizable floating windows
    │   c_yui_tabs.js        Tab container for sub-components
    │   c_yui_form.js        Form builder (Tabulator + TomSelect)
    │   c_yui_routing.js     Hash-based menu/content routing
    │   c_yui_map.js         MapLibre GL map wrapper
    │   c_yui_uplot.js       uPlot chart wrapper
    │   c_yui_json_graph.js  JSON visualization with AntV/G6
    │
    ├── TreeDB Components ──────────────────────────────────────
    │   c_yui_treedb_topics.js           Topic list management
    │   c_yui_treedb_topic_with_form.js  Table + form editor
    │   c_yui_treedb_graph.js            Graph manager / backend bridge
    │   c_g6_nodes_tree.js               G6-based node graph editor
    │
    ├── Utilities ──────────────────────────────────────────────
    │   lib_graph.js         DOM utilities (classes, enable/disable)
    │   lib_icons.js         SVG icon injection
    │   lib_maplibre.js      Custom MapLibre controls
    │   themes.js            Light/dark theme management
    │   ytable.js            Simple HTML table with selection
    │   yui_toolbar.js       Horizontal scrollable toolbar
    │   yui_dev.js           Developer tools (JSON editor, traffic)
    │
    └── CSS ────────────────────────────────────────────────────
        c_yui_main.css       Main layout and layers
        c_yui_map.css        Map styles
        c_yui_routing.css    Router layout
        lib_graph.css        Graph utility styles
        ytable.css           Table styles
        yui_toolbar.css      Toolbar styles
        yui_icons.css        Icon font styles
```

## Exports

### Components

| Export | Source | Description |
|--------|--------|-------------|
| `register_c_yui_main` | `c_yui_main.js` | Main app shell with layers and toolbar |
| `display_volatil_modal` | `c_yui_main.js` | Show a temporary modal |
| `display_info_message` | `c_yui_main.js` | Show info notification |
| `display_warning_message` | `c_yui_main.js` | Show warning notification |
| `display_error_message` | `c_yui_main.js` | Show error notification |
| `get_yesnocancel` | `c_yui_main.js` | Yes/No/Cancel dialog |
| `get_yesno` | `c_yui_main.js` | Yes/No dialog |
| `get_ok` | `c_yui_main.js` | OK dialog |
| `register_c_yui_window` | `c_yui_window.js` | Floating window container |
| `register_c_yui_tabs` | `c_yui_tabs.js` | Tab container |
| `register_c_yui_form` | `c_yui_form.js` | Form builder |
| `register_c_yui_routing` | `c_yui_routing.js` | Hash-based routing |
| `register_c_yui_map` | `c_yui_map.js` | Map component |
| `register_c_yui_uplot` | `c_yui_uplot.js` | Chart component |
| `register_c_yui_json_graph` | `c_yui_json_graph.js` | JSON graph viewer |

### TreeDB Components

| Export | Source | Description |
|--------|--------|-------------|
| `register_c_yui_treedb_topics` | `c_yui_treedb_topics.js` | Topic list UI |
| `register_c_yui_treedb_topic_with_form` | `c_yui_treedb_topic_with_form.js` | Table + form editor |
| `register_c_yui_treedb_graph` | `c_yui_treedb_graph.js` | Graph backend bridge |
| `register_c_g6_nodes_tree` | `c_g6_nodes_tree.js` | G6 node graph editor |

### Utilities

| Export | Source | Description |
|--------|--------|-------------|
| `addClasses`, `removeClasses`, `toggleClasses` | `lib_graph.js` | CSS class manipulation |
| `removeChildElements` | `lib_graph.js` | Remove all children from element |
| `disableElements`, `enableElements` | `lib_graph.js` | Enable/disable DOM elements |
| `set_submit_state`, `set_cancel_state`, `set_active_state` | `lib_graph.js` | Visual state helpers |
| `getStrokeColor` | `lib_graph.js` | Generate stroke from fill color |
| `inject_svg_icons` | `lib_icons.js` | Inject SVG icon symbols into DOM |
| `EditControl`, `MarkerControl` | `lib_maplibre.js` | Custom MapLibre GL controls |
| `themes` | `themes.js` | Theme initialization and switching |
| `YTable`, `createYTable` | `ytable.js` | HTML table with selection |
| `yui_toolbar` | `yui_toolbar.js` | Scrollable toolbar builder |
| `info_traffic`, `setup_dev` | `yui_dev.js` | Developer panel and traffic monitor |

## Skeleton — Starting a New Project

Copy the template files from `skeleton/` to bootstrap a new GUI:

```bash
cp lib-yui/skeleton/index.html  my-gui/index.html
cp lib-yui/skeleton/config.json my-gui/config.json
```

### `index.html`

A clean, invariant HTML file with three placeholders replaced at build time:

| Placeholder | Replaced with | Source |
|-------------|---------------|--------|
| `<!-- CSP_PLACEHOLDER -->` | Full `<meta http-equiv="Content-Security-Policy">` tag | `config.json → csp_connect_src` |
| `<title></title>` | `<title>My App</title>` | `config.json → title` |
| `<!-- METADATA_PLACEHOLDER -->` | `<meta name="..." content="...">` tags | `config.json → metadata` |

**You should not edit `index.html` directly.** All variable parts go in `config.json`.

### `config.json`

```json
{
    "title": "My Yuneta GUI",

    "metadata": {
        "application-name": "My Yuneta GUI",
        "description": "Yuneta-based GUI application",
        "keywords": "yuneta gui"
    },

    "csp_connect_src": [
        "wss://localhost:1800",
        "https://localhost:1801",
        "https://auth.artgins.com"
    ]
}
```

**`csp_connect_src`** must list every origin from `backend_config.js`:
- Each `backend_urls` value (`wss://...`)
- Each BFF endpoint (`https://...:port`)
- Each Keycloak `auth-server-url` (`https://...`)

Entries starting with `_comment` are ignored (used for inline documentation).

## Vite Plugin — `yunetaHtmlPlugin`

Shared plugin that replaces the placeholders in `index.html` at build time.

```js
import { yunetaHtmlPlugin } from "@yuneta/lib-yui/vite-plugin-yuneta-html.js";

// Options:
yunetaHtmlPlugin({
    defaultTitle: "My App",     // Fallback if config.json is missing
    configPath: "./config.json" // Custom path (default: ./config.json)
})
```

The plugin generates a strict Content-Security-Policy with:
- `default-src 'self'`
- `script-src 'self'`
- `style-src 'self' 'unsafe-inline'` (required by Bulma)
- `connect-src 'self' <origins from csp_connect_src>`
- `worker-src 'self' blob:`
- `child-src 'self' blob:`
- `img-src 'self' data: blob:`
- `font-src 'self' data:`

## Peer Dependencies

lib-yui does **not** bundle these — your project must include them:

| Package | Used by |
|---------|---------|
| `@yuneta/gobj-js` | All components (GClass framework) |
| `@antv/g6` | `c_g6_nodes_tree`, `c_yui_json_graph` |
| `bulma` | All components (CSS framework) |
| `i18next` | `c_yui_main`, `c_yui_form` (i18n) |
| `maplibre-gl` | `c_yui_map` |
| `tabulator-tables` | `c_yui_form`, `c_yui_treedb_topic_with_form` |
| `tom-select` | `c_yui_form`, `c_yui_treedb_topic_with_form` |
| `uplot` | `c_yui_uplot` |
| `vanilla-jsoneditor` | `c_yui_treedb_topic_with_form`, `yui_dev` |
