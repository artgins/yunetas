# gobj-js

JavaScript/ES6 implementation of the [Yuneta](https://yuneta.io) framework (v7).

Yuneta is a **function-oriented**, event-driven framework for building distributed systems. Components are built from plain functions and data tables — not class hierarchies — making the framework portable to any programming language. This package provides the full GObject + Finite State Machine (FSM) runtime for the browser and Node.js environments.

Published as [`@yuneta/gobj-js`](https://www.npmjs.com/package/@yuneta/gobj-js).

## License

Licensed under the [MIT License](http://www.opensource.org/licenses/mit-license).

---

## Table of Contents

- [Function-Oriented & Language-Portable](#function-oriented--language-portable)
- [Architecture Overview](#architecture-overview)
- [Installation](#installation)
- [Build & Develop](#build--develop)
- [Publish](#publish)
- [Quick Start](#quick-start)
- [Core Concepts](#core-concepts)
- [Public API](#public-api)
- [Built-in GClasses](#built-in-gclasses)
- [TreeDB Helpers](#treedb-helpers)
- [Writing a Custom GClass](#writing-a-custom-gclass)
- [Source Layout](#source-layout)

---

## Function-Oriented & Language-Portable

Yuneta is designed around a **function-oriented paradigm**. Every component (GClass) is defined by wiring together plain functions and data tables:

- **Action functions** handle events: `function ac_connect(gobj, event, kw, src)`
- **Lifecycle methods** manage creation and teardown: `mt_create`, `mt_start`, `mt_stop`, `mt_destroy`
- **FSM tables** map (state, event) pairs to action functions and next-state transitions
- **Attribute schemas** (`SDATA`) declare typed, flagged fields as data — not getter/setter methods

There is no class inheritance, no method overriding, and no language-specific constructs at the core. A GClass is a bag of functions plus a data description, registered at startup.

### Why this matters

This design makes Yuneta **portable to any programming language** that supports functions, arrays, and structured data — which is virtually all of them. The reference implementation is in **C** (~12,000 LOC in `gobj.c`), and this JavaScript package mirrors the same API and patterns:

| Concept | C | JavaScript |
|---------|---|------------|
| Create an instance | `gobj_create(name, gclass, kw, parent)` | `gobj_create(name, gclass_name, attrs, parent)` |
| Send an event | `gobj_send_event(gobj, event, kw, src)` | `gobj_send_event(gobj, event, kw, src)` |
| Subscribe | `gobj_subscribe_event(pub, event, kw, sub)` | `gobj_subscribe_event(pub, event, kw, sub)` |
| Attribute schema | `SDATA(DTP_STRING, "url", SDF_RD, "", "desc")` | `SDATA(DTP_STRING, "url", SDF_RD, "", "desc")` |
| FSM action row | `{EV_CONNECT, ac_connect, "ST_CONNECTED"}` | `["EV_CONNECT", ac_connect, "ST_CONNECTED"]` |

The same GClass structure translates directly between languages. Different implementations can also **interoperate over the network** via the inter-event protocol: the built-in `C_IEVENT_CLI` GClass proxies a remote Yuneta service over WebSocket, so a JavaScript frontend can communicate with a C backend as if it were a local GObject.

---

## Architecture Overview

```
┌──────────────────────────────────────────────────────────┐
│                         Yuno                             │
│   (application root GObject)                             │
│                                                          │
│   ┌──────────────┐   ┌──────────────┐                    │
│   │   Service A  │   │   Service B  │                    │
│   │  (GObject)   │   │  (GObject)   │                    │
│   │              │   │              │                    │
│   │  ┌────────┐  │   │  ┌────────┐  │                    │
│   │  │Child 1 │  │   │  │Child 2 │  │                    │
│   │  └────────┘  │   │  └────────┘  │                    │
│   └──────────────┘   └──────────────┘                    │
└──────────────────────────────────────────────────────────┘
         │ events (JSON kw)  ↕  pub/sub
```

Every component is a **GObject** — an instance of a **GClass**. GClasses define:
- Typed **attributes** (schema via `SDATA`)
- A **Finite State Machine** (states + event-action table)
- **Lifecycle methods** (`mt_create`, `mt_start`, `mt_stop`, `mt_destroy`, …)

GObjects communicate exclusively via **events** carrying JSON key-value payloads (`kw`). There is no direct method calling between components — everything goes through the FSM event dispatcher.

---

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

---

## Build & Develop

Requires Node.js LTS (v22+):

```bash
nvm install --lts
npm install -g vite
```

```bash
cd gobj-js/
npm install        # install dependencies

vite               # dev server
vite build         # build all output formats to dist/
npm test           # run tests (vitest)
npm run test:coverage
npx vitest --watch # watch mode
```

To update dependencies:

```bash
npm install -g npm-check-updates
ncu -u && npm install
```

### Output formats

`vite build` produces files in `dist/`:

| File | Format | Use |
|------|--------|-----|
| `gobj-js.es.js` | ES modules | bundlers, modern browsers |
| `gobj-js.cjs.js` | CommonJS | Node.js |
| `gobj-js.umd.js` | UMD | legacy bundlers |
| `gobj-js.iife.js` | IIFE | `<script>` tag |
| `*.min.js` | minified variants | production |

---

## Publish

To publish a new version of `@yuneta/gobj-js` to [npmjs.com](https://www.npmjs.com/package/@yuneta/gobj-js):

```bash
# 1. Configure your npm token (only once)
echo "//registry.npmjs.org/:_authToken=<your-token>" > ~/.npmrc

# 2. Update the version in package.json
npm version patch   # or minor / major

# 3. Publish (build runs automatically via prepublishOnly)
npm publish --access public
```

To create an npm token, go to [npmjs.com](https://www.npmjs.com) → Account → Access Tokens.

---

## Quick Start

```javascript
import {
    gobj_start_up,
    gobj_create_yuno,
    gobj_create_service,
    gobj_start,
    gobj_play,
    gobj_yuno,
    db_load_persistent_attrs,
    db_save_persistent_attrs,
    db_remove_persistent_attrs,
    db_list_persistent_attrs,
    register_c_yuno,
    register_c_timer,
    register_c_ievent_cli,
} from "@yuneta/gobj-js";

// 1. Register GClasses
register_c_yuno();
register_c_timer();
register_c_ievent_cli();

// 2. Initialize framework
gobj_start_up(
    null,                           // jn_global_settings (JSON)
    db_load_persistent_attrs,       // load  persistent attrs
    db_save_persistent_attrs,       // save  persistent attrs
    db_remove_persistent_attrs,     // remove persistent attrs
    db_list_persistent_attrs,       // list  persistent attrs
    null,                           // global command parser fn
    null                            // global stats parser fn
);

// 3. Create the Yuno (application root)
let yuno = gobj_create_yuno("yuno", "C_YUNO", {
    yuno_name: "my_app",
    yuno_role: "my_role",
    yuno_version: "1.0.0",
});

// 4. Create services under the yuno
gobj_create_service("main", "C_MY_SERVICE", {}, gobj_yuno());

// 5. Start everything
gobj_start(yuno);
gobj_play(yuno);   // triggers mt_play on the default service
```

---

## Core Concepts

### GClass & GObject

A **GClass** is a class definition — registered once at startup. A **GObject** is an instance of a GClass.

```javascript
// Register a GClass
gclass_create(name, event_types, states, gmt, lmt,
              attrs_table, private_data, authz_table,
              command_table, s_user_trace_level, gclass_flag);

// Create an instance
let gobj = gobj_create(name, gclass_name, attributes, parent);
```

### Finite State Machines

Each GClass defines:
- A list of **states** (strings, e.g. `"ST_IDLE"`, `"ST_CONNECTED"`)
- Per-state **event-action tables**: `[event_name, action_fn, next_state]`

When an event is sent to a GObject, the FSM looks up the current state → finds the matching event → calls the action function. `next_state` (or `null` to stay) controls state transitions.

```javascript
const st_idle = [
    ["EV_CONNECT",    ac_connect,    "ST_CONNECTED"],
    ["EV_TIMEOUT",    ac_timeout,    null],           // stay in ST_IDLE
];
const st_connected = [
    ["EV_DISCONNECT", ac_disconnect, "ST_IDLE"],
    ["EV_MESSAGE",    ac_message,    null],
];

const states = [
    ["ST_IDLE",      st_idle],
    ["ST_CONNECTED", st_connected],
];
```

### Attributes (SData)

Attributes are declared in a schema table using `SDATA()` macros. Each attribute has a type, name, flags, default value, and description.

```javascript
import { SDATA, SDATA_END, data_type_t, sdata_flag_t } from "@yuneta/gobj-js";

const attrs_table = [
    SDATA(data_type_t.DTP_STRING,  "url",        sdata_flag_t.SDF_RD,      "", "Server URL"),
    SDATA(data_type_t.DTP_INTEGER, "timeout",    sdata_flag_t.SDF_RD,       0, "Timeout ms"),
    SDATA(data_type_t.DTP_BOOLEAN, "connected",  sdata_flag_t.SDF_RD,   false, "Connection state"),
    SDATA(data_type_t.DTP_STRING,  "saved_key",  sdata_flag_t.SDF_PERSIST,  "", "Persisted value"),
    SDATA_END()
];
```

**Data types (`data_type_t`):** `DTP_STRING`, `DTP_BOOLEAN`, `DTP_INTEGER`, `DTP_REAL`, `DTP_LIST`, `DTP_DICT`, `DTP_JSON`, `DTP_POINTER`

**Flags (`sdata_flag_t`):** `SDF_RD`, `SDF_WR`, `SDF_PERSIST`, `SDF_STATS`, `SDF_AUTHZ_R`, `SDF_AUTHZ_W`, `SDF_REQUIRED`, `SDF_VOLATIL`

### Events & Pub-Sub

GObjects communicate via events with JSON payloads:

```javascript
// Send directly to a specific GObject
gobj_send_event(target_gobj, "EV_CONNECT", { url: "ws://..." }, src_gobj);

// Publish to all subscribers
gobj_publish_event(gobj, "EV_DATA_READY", { data: [] });

// Subscribe to events from a source
gobj_subscribe_event(source_gobj, "EV_DATA_READY", {}, subscriber_gobj);

// Unsubscribe
gobj_unsubscribe_event(source_gobj, "EV_DATA_READY", {}, subscriber_gobj);
```

### GObject Tree (Yuno)

GObjects form a parent-child tree. The root is the **Yuno**. Services live directly under the Yuno. Each GObject has exactly one parent (except the Yuno itself).

```
Yuno
 ├── Service "auth"     (C_IEVENT_CLI)
 ├── Service "main"     (C_MY_SERVICE)
 │    └── Child "sub"   (C_SOME_GCLASS)
 └── Service "timer"    (C_TIMER)
```

---

## Public API

### Framework Bootstrap

```javascript
gobj_start_up(
    jn_global_settings,         // JSON object or null
    load_persistent_attrs_fn,   // fn(gobj, keys)
    save_persistent_attrs_fn,   // fn(gobj, keys)
    remove_persistent_attrs_fn, // fn(gobj, keys)
    list_persistent_attrs_fn,   // fn(gobj, keys)
    global_command_parser_fn,   // fn or null
    global_stats_parser_fn      // fn or null
)
```

### GClass Registration

```javascript
gclass_create(name, event_types, states, gmt, lmt, attrs_table,
              private_data, authz_table, command_table,
              s_user_trace_level, gclass_flag)

gclass_find_by_name(name)        // → GClass or null
gclass_check_fsm(gclass)         // validate FSM (returns error count)
gclass_add_event_type(gclass, event_name, flag)
gclass_add_state(gclass, state_name)
gclass_add_ev_action(gclass, state_name, event_name, action_fn, next_state)
```

### GObject Lifecycle

```javascript
// Creation
gobj_create(name, gclass_name, attrs, parent)       // generic child
gobj_create_yuno(name, gclass_name, attrs)           // application root
gobj_create_service(name, gclass_name, attrs, yuno)  // named service
gobj_create_default_service(name, gclass_name, attrs, yuno)
gobj_create_volatil(name, gclass_name, attrs, parent)
gobj_create_pure_child(name, gclass_name, attrs, parent)

// Start / Stop
gobj_start(gobj)           // calls mt_start
gobj_stop(gobj)            // calls mt_stop
gobj_start_children(gobj)
gobj_stop_children(gobj)
gobj_start_tree(gobj)
gobj_stop_tree(gobj)

// Play / Pause (applied to default service)
gobj_play(gobj)
gobj_pause(gobj)

// Destroy
gobj_destroy(gobj)

// Status
gobj_is_running(gobj)      // → boolean
gobj_is_playing(gobj)      // → boolean
gobj_is_destroying(gobj)   // → boolean
gobj_is_volatil(gobj)      // → boolean
gobj_is_pure_child(gobj)   // → boolean
```

### State Machine

```javascript
gobj_current_state(gobj)                         // → state name string
gobj_change_state(gobj, new_state)               // trigger FSM transition
gobj_has_event(gobj, event, event_flag)          // → boolean; pass 0 to ignore flag
gobj_has_output_event(gobj, event, event_flag)   // same; checks EVF_OUTPUT_EVENT
```

### Attribute Access

```javascript
// Generic read/write
// `path` accepts back-tick navigation to walk into child gobjs: "child`subattr"
gobj_read_attr (gobj, name, src)
gobj_write_attr(gobj, path, value, src)
gobj_read_attrs (gobj, include_flag, src)                // → JSON object of matching attrs
gobj_write_attrs(gobj, kw, include_flag, src)            // include_flag filters which are written
gobj_has_attr(gobj, name)                                // → boolean

// Typed reads — name only (no src)
gobj_read_bool_attr   (gobj, name)
gobj_read_integer_attr(gobj, name)
gobj_read_str_attr    (gobj, name)
gobj_read_pointer_attr(gobj, name)

// Typed writes — (name, value), no src
gobj_write_bool_attr   (gobj, name, value)
gobj_write_integer_attr(gobj, name, value)
gobj_write_str_attr    (gobj, name, value)
```

### Event System

```javascript
// Sending
gobj_send_event(gobj, event, kw, src)         // direct send
gobj_publish_event(gobj, event, kw)           // publish to subscribers
gobj_post_event(gobj, event, kw, src)         // post (deferred)

// Subscriptions
gobj_subscribe_event  (publisher, event, kw, subscriber)
gobj_unsubscribe_event(publisher, event, kw, subscriber)
gobj_unsubscribe_list (publisher, dl_subs, force)     // force=true also removes hard subs
gobj_find_subscriptions(publisher, event, kw, subscriber)
gobj_list_subscriptions(gobj)
gobj_find_subscribings (subscriber, event, kw, publisher)

// Commands & Stats
gobj_command(gobj, command, kw, src)          // → response JSON
gobj_stats(gobj, stats, kw, src)              // → response JSON
build_command_response(gobj, result, comment, schema, data)
build_stats_response(gobj, result, comment, schema, data)
```

### Hierarchy & Navigation

```javascript
gobj_parent(gobj)
gobj_yuno()                        // → the Yuno root
gobj_default_service()             // → default service under yuno

gobj_name(gobj)
gobj_short_name(gobj)
gobj_full_name(gobj)
gobj_gclass_name(gobj)
gobj_yuno_name(gobj)
gobj_yuno_role(gobj)
gobj_yuno_id(gobj)

gobj_find_child(gobj, kw_filter)
gobj_find_service(name, verbose)
gobj_find_gobj(gobj, path)
gobj_search_path(gobj, path)

gobj_walk_gobj_children(gobj, walk_type, cb_walking, user_data, user_data2)
gobj_walk_gobj_children_tree(gobj, walk_type, cb_walking, user_data, user_data2)
```

### Persistence

Backed by `localStorage` (browser) via `dbsimple.js`:

```javascript
// Low-level (dbsimple)
db_load_persistent_attrs(gobj, keys)
db_save_persistent_attrs(gobj, keys)
db_remove_persistent_attrs(gobj, keys)
db_list_persistent_attrs(gobj, keys)

// Via gobj (delegates to registered persistence fns)
gobj_load_persistent_attrs(gobj, keys)
gobj_save_persistent_attrs(gobj, keys)
gobj_remove_persistent_attrs(gobj, keys)
gobj_list_persistent_attrs(gobj, keys)
```

Attributes marked `SDF_PERSIST` are automatically saved/loaded.

### Helpers & Utilities

```javascript
// JSON / Object operations
json_deep_copy(obj)
json_is_identical(a, b)
json_object_update(dst, src)            // merge src into dst
json_object_update_existing(dst, src)   // only existing keys
json_object_update_missing(dst, src)    // only missing keys
json_object_get(obj, key)
json_object_set(obj, key, value)
json_object_del(obj, key)
json_array_append(arr, value)
json_array_remove(arr, index)
json_array_extend(dst, src)
json_object_size(obj)
json_array_size(arr)

// Type checking
is_object(v), is_array(v), is_string(v), is_number(v),
is_boolean(v), is_null(v), is_date(v), is_function(v), is_gobj(v)

// Keyword (kw) operations
//
// Most kw_* helpers take `gobj` as first argument (for error logging)
// and a dot/back-tick path instead of a single key. Exceptions that do
// NOT take gobj: kw_has_key, kw_pop, kw_match_simple.

kw_has_key(kw, key)                                      // → boolean
kw_pop(kw1, kw2)                                         // delete from kw1 the keys listed in kw2
kw_delete(gobj, kw, path)
kw_find_path(gobj, kw, path, verbose)                    // back-tick path: "a`b`c"

kw_get_bool(gobj, kw, path, default_value, flag)
kw_get_int (gobj, kw, path, default_value, flag)
kw_get_real(gobj, kw, path, default_value, flag)
kw_get_str (gobj, kw, path, default_value, flag)
kw_get_dict(gobj, kw, path, default_value, flag)
kw_get_list(gobj, kw, path, default_value, flag)
kw_get_dict_value(gobj, kw, path, default_value, flag)

kw_set_dict_value(gobj, kw, path, value)
kw_set_subdict_value(gobj, kw, path, key, value)

kw_match_simple(kw, filter)                              // → boolean
kw_select (gobj, kw, jn_filter, match_fn)                // filter list
kw_collect(gobj, kw, jn_filter, match_fn)                // extract subset
kw_clone_by_keys    (gobj, kw, keys, verbose)
kw_clone_by_not_keys(gobj, kw, keys, verbose)

// Flags: KW_REQUIRED, KW_CREATE, KW_EXTRACT, KW_RECURSIVE, KW_WILD_NUMBER

// Local storage helpers (browser localStorage, no "store" parameter)
kw_get_local_storage_value(key, default_value, create)   // create=false
kw_set_local_storage_value(key, value)
kw_remove_local_storage_value(key)

// Misc utilities
current_timestamp()          // ISO string
get_now()                    // Date object
node_uuid()                  // generate UUID
parseBoolean(v)              // coerce to boolean
empty_string(s)              // true if null/undefined/""
str_in_list(list, str, ci)   // case-insensitive optional
index_in_list(list, value)
delete_from_list(list, value)
jwtDecode(token)             // → header/payload/signature
jwt2json(token)              // → payload JSON
debounce(fn, delay)
timeTracker()

// Inter-event message stack
msg_iev_push_stack  (gobj, kw, stack, jn_data)    // push jn_data onto named stack
msg_iev_get_stack   (gobj, kw, stack, verbose)    // peek top of named stack
msg_iev_set_msg_type(gobj, kw, msg_type)          // "" to delete
msg_iev_get_msg_type(gobj, kw)
msg_iev_write_key(kw, key, value)                 // no gobj
msg_iev_read_key (kw, key)                        // no gobj
```

### Logging

All log functions take a single `format` argument and use `printf`-style
substitution (`%s`, `%d`, `%i`, `%f`, `%o`/`%O`). There is no `gobj` or
error-code argument (that's the C API — the JS runtime is simpler).

```javascript
log_error(format, ...args)       // red, prefixed "ERROR"
log_warning(format, ...args)     // yellow, prefixed "WARNING"
log_info(format, ...args)        // cyan, prefixed "INFO"
log_debug(format, ...args)       // silver, prefixed "DEBUG"
trace_msg(format, ...args)       // cyan, prefixed "MSG"
trace_json(json, msg)            // dir-dump a JSON object

// Redirect error/warning output to a single remote handler
// (info/debug always go to the browser console)
set_remote_log_functions(remote_log_fn)   // fn(message) — single function
```

### String Formatting

```javascript
sprintf(format, ...args)    // printf-style
vsprintf(fmt, argv)         // variadic version
```

Format specifiers: `%s` `%d` `%i` `%f` `%e` `%g` `%o` `%x` `%X` `%b` `%c` `%j` (JSON) `%t` (boolean) `%T` (type) `%v` (value) `%u` (unsigned)

---

## Built-in GClasses

### C_YUNO

The application root. Always the first GClass registered.

```javascript
import { register_c_yuno } from "@yuneta/gobj-js";
register_c_yuno();
```

Key attributes: `yuno_name`, `yuno_role`, `yuno_id`, `yuno_version`, `yuno_release`, `yuneta_version`, `required_services`, `tracing`, `start_date`, `node_uuid`, `__username__`

### C_TIMER

Manages timeouts and periodic timers.

```javascript
import { register_c_timer, set_timeout, set_timeout_periodic, clear_timeout } from "@yuneta/gobj-js";
register_c_timer();

// One-shot timeout (ms)
set_timeout(timer_gobj, 5000);

// Periodic timeout
set_timeout_periodic(timer_gobj, 1000);

// Cancel
clear_timeout(timer_gobj);
```

Events published: `EV_TIMEOUT`, `EV_TIMEOUT_PERIODIC`

Attributes: `subscriber`, `periodic`, `msec`

### C_IEVENT_CLI

Inter-event client — proxies a remote Yuneta service over WebSocket so it looks like a local GObject. Used to communicate with backend yunos.

```javascript
import { register_c_ievent_cli } from "@yuneta/gobj-js";
register_c_ievent_cli();

let remote = gobj_create_service("backend", "C_IEVENT_CLI", {
    url: "ws://localhost:1991",
    wanted_yuno_role: "agent",
    wanted_yuno_service: "agent",
    jwt: "...",
}, gobj_yuno());
```

Key attributes: `url`, `jwt`, `wanted_yuno_role`, `wanted_yuno_name`, `wanted_yuno_service`, `remote_yuno_role`, `remote_yuno_name`, `remote_yuno_service`

---

## TreeDB Helpers

Utilities for interacting with the Yuneta TreeDB (schema-driven graph database):

```javascript
import {
    treedb_hook_data_size,
    treedb_decoder_fkey,
    treedb_encoder_fkey,
    treedb_decoder_hook,
    treedb_get_field_desc,
    template_get_field_desc,
    create_template_record,
} from "@yuneta/gobj-js";

treedb_hook_data_size(value)                // count with caching
treedb_decoder_fkey(col, fkey)              // parse foreign key reference
treedb_encoder_fkey(col, fkey)              // build fkey string "topic^id^hook"
treedb_decoder_hook(col, hook)              // parse hook reference
treedb_get_field_desc(col)                  // build field descriptor from column
template_get_field_desc(key, value)         // build field descriptor from template
create_template_record(template, kw)        // instantiate from template
```

---

## Writing a Custom GClass

```javascript
import {
    SDATA, SDATA_END,
    data_type_t, sdata_flag_t,
    gclass_create,
    gobj_subscribe_event, gobj_yuno,
    trace_msg,
} from "@yuneta/gobj-js";

const GCLASS_NAME = "C_MY_CLASS";

// 1. Attribute schema
const attrs_table = [
    SDATA(data_type_t.DTP_STRING,  "url",      sdata_flag_t.SDF_RD, "", "Remote URL"),
    SDATA(data_type_t.DTP_INTEGER, "retries",  sdata_flag_t.SDF_RD,  3, "Max retries"),
    SDATA_END()
];

// 2. Private data (per-instance, copied from this template)
const PRIVATE_DATA = {
    retry_count: 0,
};

// 3. Lifecycle methods
function mt_create(gobj) {
    // instance created, attrs not yet populated
}
function mt_start(gobj) {
    // subscribe to events from other gobjs here
    gobj_subscribe_event(gobj_yuno(), "EV_TIMEOUT_PERIODIC", {}, gobj);
}
function mt_stop(gobj) { }
function mt_destroy(gobj) { }

// 4. Action functions
function ac_timeout(gobj, event, kw, src) {
    trace_msg(`Tick! retry_count=${gobj.priv.retry_count}`);
    gobj.priv.retry_count++;
    return 0;  // kw consumed
}

// 5. FSM
const st_idle = [
    ["EV_TIMEOUT_PERIODIC", ac_timeout, null],
];
const states = [
    ["ST_IDLE", st_idle],
];
const event_types = [
    ["EV_TIMEOUT_PERIODIC", 0],
];

// 6. Methods table
const gmt = { mt_create, mt_start, mt_stop, mt_destroy };

// 7. Register
function register_c_my_class() {
    return gclass_create(
        GCLASS_NAME,
        event_types,
        states,
        gmt,
        0,              // lmt (low-level methods)
        attrs_table,
        PRIVATE_DATA,
        0,              // authz_table
        0,              // command_table
        0,              // s_user_trace_level
        0               // gclass_flag
    );
}

export { register_c_my_class };
```

---

## Source Layout

```
kernel/js/gobj-js/
├── README.md                          ← this file
├── package.json
├── vite.config.js
├── dist/                              ← compiled outputs (ES, CJS, UMD, IIFE)
├── tests/
│   └── kw_delete.test.js
└── src/
    ├── index.js                       ← public API entry point (barrel export)
    ├── gobj.js                        ← GObject/FSM runtime      (~4 500 LOC)
    ├── helpers.js                     ← utilities, JSON, logging (~3 300 LOC)
    ├── c_ievent_cli.js                ← remote service proxy     (~1 350 LOC)
    ├── lib_treedb.js                  ← TreeDB helpers           (~  540 LOC)
    ├── c_timer.js                     ← Timer GClass             (~  330 LOC)
    ├── c_yuno.js                      ← Yuno GClass              (~  290 LOC)
    ├── dbsimple.js                    ← localStorage persistence (~  140 LOC)
    ├── sprintf.js                     ← printf-style formatting  (~  210 LOC)
    ├── command_parser.js              ← command parsing
    └── stats_parser.js                ← stats parsing
```
