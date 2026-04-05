# Yuneta JavaScript Packages

JavaScript/ES6 implementation of the [Yuneta](https://yuneta.io) framework (v7).
Built with [Vite](https://vite.dev/), requires Node.js LTS (v22+).

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

### [`lib-yui`](lib-yui/) — UI Component Library

Reusable GUI components for Yuneta-based web applications.
Published as [`@yuneta/lib-yui`](https://www.npmjs.com/package/@yuneta/lib-yui).

Provides:
- App shell, floating windows, tabs, forms, routing
- MapLibre GL map wrapper, uPlot charts, JSON graph visualization
- TreeDB topic management and graph editing components
- Theme management, toolbar, developer tools
- Vite plugin for CSP generation and HTML templating
- Project skeleton for bootstrapping new GUIs

See [`lib-yui/README.md`](lib-yui/README.md) for the component guide and setup instructions.

## Quick Start

```bash
# Install Node.js LTS
nvm install --lts
npm install -g vite

# Build gobj-js
cd gobj-js && npm install && vite build && cd ..

# Build lib-yui
cd lib-yui && npm install && vite build && cd ..
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
│       ├── gobj.js        ← GObject/FSM runtime      (~4 500 LOC)
│       ├── helpers.js     ← utilities, JSON, logging  (~3 300 LOC)
│       ├── c_ievent_cli.js← remote service proxy      (~1 350 LOC)
│       ├── lib_treedb.js  ← TreeDB helpers            (~  540 LOC)
│       ├── c_timer.js     ← Timer GClass              (~  330 LOC)
│       ├── c_yuno.js      ← Yuno GClass               (~  290 LOC)
│       ├── dbsimple.js    ← localStorage persistence  (~  140 LOC)
│       ├── sprintf.js     ← printf-style formatting   (~  210 LOC)
│       ├── command_parser.js
│       └── stats_parser.js
└── lib-yui/               ← @yuneta/lib-yui npm package
    ├── README.md          ← component guide & setup
    ├── package.json
    ├── index.js           ← barrel re-exports
    ├── vite.config.js
    ├── vite-plugin-yuneta-html.js
    ├── skeleton/          ← templates for new projects
    └── src/               ← UI components (27 files)
```
