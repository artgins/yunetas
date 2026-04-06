# JavaScript API

JavaScript / ES6 implementation of the Yuneta framework for the browser
and Node.js. Published on npm as `@yuneta/gobj-js`.

Yuneta is **function-oriented** and **language-portable**: a GClass is a
bag of plain functions plus a data description, registered at startup.
There is no class inheritance, no method overriding, and no
language-specific constructs at the core. The JavaScript package mirrors
the same API and patterns as the C reference implementation
(`kernel/c/gobj-c`) so the two stacks can interoperate over the network.

## Why the same API on both sides

| Concept | C | JavaScript |
|---------|---|------------|
| Create an instance | `gobj_create(name, gclass, kw, parent)` | `gobj_create(name, gclass_name, attrs, parent)` |
| Send an event | `gobj_send_event(gobj, event, kw, src)` | `gobj_send_event(gobj, event, kw, src)` |
| Subscribe | `gobj_subscribe_event(pub, event, kw, sub)` | `gobj_subscribe_event(pub, event, kw, sub)` |
| Attribute schema | `SDATA(DTP_STRING, "url", SDF_RD, "", "desc")` | `SDATA(DTP_STRING, "url", SDF_RD, "", "desc")` |
| FSM action row | `{EV_CONNECT, ac_connect, "ST_CONNECTED"}` | `["EV_CONNECT", ac_connect, "ST_CONNECTED"]` |

The same GClass structure translates directly between languages.
Different implementations can **interoperate over the network** via the
inter-event protocol: the built-in `C_IEVENT_CLI` GClass proxies a
remote Yuneta service over WebSocket, so a JavaScript frontend can
communicate with a C backend as if it were a local GObject.

## Installation

```bash
# From npm (published package)
npm install @yuneta/gobj-js

# From source (local)
npm install /path/to/kernel/js/gobj-js
```

Import in your project:

```javascript
import { gobj_start_up, gobj_create_yuno, register_c_yuno } from "@yuneta/gobj-js";
```

Browse the JavaScript API pages in the left sidebar, in the order
shown. They follow the same sequence as the C reference: bootstrap →
GClass registration → GObject lifecycle → state machine → attributes →
events → hierarchy → persistence → helpers → logging → built-in
GClasses → TreeDB helpers.

## Source layout

```
kernel/js/gobj-js/
├── src/
│   ├── index.js            ← public API entry point (barrel export)
│   ├── gobj.js             ← GObject/FSM runtime      (~4 500 LOC)
│   ├── helpers.js          ← utilities, JSON, logging (~3 300 LOC)
│   ├── c_ievent_cli.js     ← remote service proxy     (~1 350 LOC)
│   ├── lib_treedb.js       ← TreeDB helpers           (~  540 LOC)
│   ├── c_timer.js          ← Timer GClass             (~  330 LOC)
│   ├── c_yuno.js           ← Yuno GClass              (~  290 LOC)
│   ├── dbsimple.js         ← localStorage persistence (~  140 LOC)
│   ├── sprintf.js          ← printf-style formatting  (~  210 LOC)
│   ├── command_parser.js   ← command parsing
│   └── stats_parser.js     ← stats parsing
├── tests/
└── dist/                   ← compiled outputs (ES, CJS, UMD, IIFE)
```
