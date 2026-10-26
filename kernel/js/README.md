# Yuneta JavaScript Packages

JavaScript/ES6 implementation of the [Yuneta](https://yuneta.io) framework (v7).
Built with [Vite](https://vite.dev/), requires Node.js LTS (v22+).

> **Both packages are git submodules.** `gobj-js` and `gobj-ui` each live in
> their own repository (`github.com/artgins/gobj-js`,
> `github.com/artgins/gobj-ui.js`) and are embedded here at `kernel/js/gobj-js`
> and `kernel/js/gobj-ui`. Clone yunetas with `--recurse-submodules` (or run
> `git submodule update --init`) or these directories will be empty. To ship a
> new version: edit the submodule directly, commit + `npm publish` in the
> standalone repo, then bump the submodule pointer in yunetas.

## Packages

### [`gobj-js`](gobj-js/) — Core Framework

The GObject + Finite State Machine (FSM) runtime for browser and Node.js.
Published as [`@yuneta/gobj-js`](https://www.npmjs.com/package/@yuneta/gobj-js).

Provides:
- GClass registration and GObject lifecycle
- Finite State Machine engine (states, events, actions)
- Typed attribute schemas (`SDATA`)
- Event system with pub/sub
- Hierarchical GObject tree (Yuno)
- Persistence via `localStorage`
- Inter-event client (`C_IEVENT_CLI`) for WebSocket communication with C backends
- TreeDB helpers for schema-driven graph databases
- Utilities: JSON operations, keyword helpers, logging, string formatting

See [`gobj-js/README.md`](gobj-js/README.md) for the full API reference.

### [`gobj-ui`](gobj-ui/) — UI Component Library

Reusable GUI components for Yuneta-based web applications.
Published as [`@yuneta/gobj-ui`](https://www.npmjs.com/package/@yuneta/gobj-ui).

Provides:
- The declarative shell (`C_YUI_SHELL`, `C_YUI_NAV`, `C_YUI_PAGER`,
  `C_YUI_WIZARD`), floating windows, forms, dialogs
- MapLibre GL map wrapper, uPlot charts, JSON graph visualization
- TreeDB topic management and graph editing components
- Theme management, toolbar, developer tools
- Vite plugin for CSP generation and HTML templating

The scaffold for a new declarative-shell GUI is the `gui/` of the `project`
templates of `yuno-skeleton`
([`utils/c/yuno-skeleton/skeletons/project_base/gui/`](../../utils/c/yuno-skeleton/skeletons/project_base/gui/)).

See [`gobj-ui/README.md`](gobj-ui/README.md) for the component guide and setup instructions.

## Quick Start

```bash
# Install Node.js LTS
nvm install --lts
npm install -g vite

# Build gobj-js
cd gobj-js && npm install && vite build && cd ..

# Build gobj-ui
cd gobj-ui && npm install && vite build && cd ..
```

## Directory Layout

```
kernel/js/
├── README.md              ← this file
├── gobj-js/               ← @yuneta/gobj-js npm package
│   ├── README.md          ← full API reference
│   ├── package.json
│   ├── vite.config.js
│   └── src/
│       ├── index.js       ← public API entry point
│       ├── gobj.js        ← GObject/FSM runtime      (~5 600 LOC)
│       ├── helpers.js     ← utilities, JSON, logging  (~4 700 LOC)
│       ├── c_ievent_cli.js← remote service proxy      (~1 600 LOC)
│       ├── lib_treedb.js  ← TreeDB helpers            (~  580 LOC)
│       ├── c_timer.js     ← Timer GClass              (~  390 LOC)
│       ├── c_yuno.js      ← Yuno GClass               (~  530 LOC)
│       ├── dbsimple.js    ← localStorage persistence  (~  140 LOC)
│       ├── sprintf.js     ← printf-style formatting   (~  210 LOC)
│       ├── command_parser.js
│       └── stats_parser.js
└── gobj-ui/               ← @yuneta/gobj-ui npm package
    ├── README.md          ← component guide & setup
    ├── package.json
    ├── index.js           ← barrel re-exports
    ├── vite.config.js
    ├── vite-plugin-yuneta-html.js
    ├── test-app/          ← a small SPA that mounts the components
    ├── test/              ← vitest suite
    └── src/               ← UI components (~90 modules)
```
