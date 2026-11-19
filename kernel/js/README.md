# Yuneta JavaScript Packages

This directory has the JavaScript/ES6 implementation of the [Yuneta](https://yuneta.io) framework (v7).
The packages are built with [Vite](https://vite.dev/) and need Node.js LTS (v22+).

Both packages are git submodules. `gobj-js` and `gobj-ui` each have their own
repository (`github.com/artgins/gobj-js`, `github.com/artgins/gobj-ui.js`).
This repository embeds them at `kernel/js/gobj-js` and `kernel/js/gobj-ui`.

> Clone yunetas with `--recurse-submodules`, or run
> `git submodule update --init`. If you do not, these directories are empty.

To ship a new version of a package:

1. Edit the submodule directly.
2. In the standalone repository, commit and run `npm publish`.
3. In yunetas, bump the submodule pointer.

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

## Packages

### [`gobj-js`](gobj-js/): Core Framework

This package is the GObject and Finite State Machine (FSM) runtime for the browser and Node.js.
It is published as [`@yuneta/gobj-js`](https://www.npmjs.com/package/@yuneta/gobj-js).

It has:
- GClass registration and GObject lifecycle
- Finite State Machine engine (states, events, actions)
- Typed attribute schemas (`SDATA`)
- Event system with pub/sub
- Hierarchical GObject tree (Yuno)
- Persistence via `localStorage`
- Inter-event client (`C_IEVENT_CLI`) for WebSocket communication with C backends
- TreeDB helpers for schema-driven graph databases
- Utilities: JSON operations, keyword helpers, logging, string formatting

[`gobj-js/README.md`](gobj-js/README.md) has the full API reference.

### [`gobj-ui`](gobj-ui/): UI Component Library

This package has reusable GUI components for web applications built on Yuneta.
It is published as [`@yuneta/gobj-ui`](https://www.npmjs.com/package/@yuneta/gobj-ui).

It has:
- The declarative shell (`C_YUI_SHELL`, `C_YUI_NAV`, `C_YUI_PAGER`,
  `C_YUI_WIZARD`), floating windows, forms, dialogs
- MapLibre GL map wrapper, uPlot charts, JSON graph visualization
- TreeDB topic management and graph editing components
- Theme management, toolbar, developer tools
- Vite plugin for CSP generation and HTML templating

The scaffold for a new declarative-shell GUI is the `gui/` of the `project`
templates of `yuno-skeleton`
([`utils/c/yuno-skeleton/skeletons/project_base/gui/`](../../utils/c/yuno-skeleton/skeletons/project_base/gui/)).

[`gobj-ui/README.md`](gobj-ui/README.md) has the component guide and the setup instructions.

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
