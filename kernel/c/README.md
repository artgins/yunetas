# kernel/c

C reference implementation of the Yuneta framework kernel.

Yuneta is a **function-oriented**, event-driven framework for building distributed systems. Components are built from plain functions and data tables — not class hierarchies — making the framework portable to any programming language. This directory contains the full GObject + Finite State Machine (FSM) runtime, async I/O engine, time-series database, TLS abstraction, JWT, and all Linux runtime GClasses.

## License

Licensed under the [MIT License](http://www.opensource.org/licenses/mit-license).

---

## Table of Contents

- [Function-Oriented & Language-Portable](#function-oriented--language-portable)
- [Architecture Overview](#architecture-overview)
- [Build](#build)
- [Module Map](#module-map)
- [gobj-c — GObject Framework](#gobj-c--gobject-framework)
  - [Core Concepts](#core-concepts)
  - [GClass Definition Macros](#gclass-definition-macros)
  - [GClass Management API](#gclass-management-api)
  - [Attribute Schema (SData)](#attribute-schema-sdata)
  - [GObject Lifecycle API](#gobject-lifecycle-api)
  - [Persistence API](#persistence-api)
  - [State Machine API](#state-machine-api)
  - [Attribute Access API](#attribute-access-api)
  - [Stats API](#stats-api)
  - [Event System API](#event-system-api)
  - [Hierarchy & Navigation](#hierarchy--navigation)
  - [Authorization](#authorization)
  - [Tracing & Debugging](#tracing--debugging)
  - [Helpers & Utilities](#helpers--utilities)
  - [Logging](#logging)
  - [GBuffer — Growable Buffer](#gbuffer--growable-buffer)
  - [Resource API](#resource-api)
  - [Node API (TreeDB operations via GObj)](#node-api-treedb-operations-via-gobj)
  - [Memory Management (gbmem)](#memory-management-gbmem)
  - [Rotatory Log Files](#rotatory-log-files)
  - [Testing Utilities](#testing-utilities)
- [yev_loop — Async Event Loop](#yev_loop--async-event-loop)
- [timeranger2 — Time-Series Database](#timeranger2--time-series-database)
  - [Queue (tr_queue)](#queue-tr_queue)
  - [Message Instances (tr_msg)](#message-instances-tr_msg)
  - [File System Watcher (fs_watcher)](#file-system-watcher-fs_watcher)
- [ytls — TLS Abstraction](#ytls--tls-abstraction)
- [libjwt — JSON Web Tokens](#libjwt--json-web-tokens)
- [root-linux — Runtime GClasses](#root-linux--runtime-gclasses)
  - [Entry Point](#entry-point)
  - [Yuno Extras](#yuno-extras)
- [root-esp32 — ESP32 Port](#root-esp32--esp32-port)
- [linux-ext-libs — External Dependencies](#linux-ext-libs--external-dependencies)
- [Writing a Custom GClass](#writing-a-custom-gclass)
- [Source Layout](#source-layout)

---

## Function-Oriented & Language-Portable

Yuneta is designed around a **function-oriented paradigm**. Every component (GClass) is defined by wiring together plain functions and data tables:

- **Action functions** handle events: `json_t *ac_connect(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)`
- **Lifecycle methods** manage creation and teardown: `mt_create`, `mt_start`, `mt_stop`, `mt_destroy`
- **FSM tables** map (state, event) pairs to action functions and next-state transitions
- **Attribute schemas** (`SDATA`) declare typed, flagged fields as data — not getter/setter methods

There is no class inheritance, no method overriding, and no language-specific constructs at the core. A GClass is a bag of functions plus a data description, registered at startup.

### Why this matters

This design makes Yuneta **portable to any programming language** that supports functions, arrays, and structured data. The C implementation here is the reference (~12,000 LOC in `gobj.c`), and the JavaScript package (`kernel/js/yunetas-js7`) mirrors the same API and patterns:

| Concept | C | JavaScript |
|---------|---|------------|
| Create an instance | `gobj_create(name, C_FOO, kw, parent)` | `gobj_create(name, "C_FOO", attrs, parent)` |
| Send an event | `gobj_send_event(gobj, EV_FOO, kw, src)` | `gobj_send_event(gobj, "EV_FOO", kw, src)` |
| Subscribe | `gobj_subscribe_event(pub, EV_FOO, kw, sub)` | `gobj_subscribe_event(pub, "EV_FOO", kw, sub)` |
| Attribute schema | `SDATA(DTP_STRING, "url", SDF_RD, "", "desc")` | `SDATA(DTP_STRING, "url", SDF_RD, "", "desc")` |
| FSM action row | `{EV_CONNECT, ac_connect, ST_CONNECTED}` | `["EV_CONNECT", ac_connect, "ST_CONNECTED"]` |
| Register a GClass | `GOBJ_DEFINE_GCLASS(C_FOO)` + populate | `gclass_create("C_FOO", events, states, gmt, ...)` |

A developer who knows the C implementation can read the JavaScript version (and vice versa) because the architecture is identical; only the syntax changes.

Different implementations can also **interoperate over the network** via the inter-event protocol. The built-in `C_IEVENT_CLI` / `C_IEVENT_SRV` GClasses proxy remote Yuneta services over WebSocket, so a JavaScript frontend can communicate with a C backend as if it were a local GObject.

---

## Architecture Overview

```
┌──────────────────────────────────────────────────────────┐
│                         Yuno                             │
│   (application root GObject)                             │
│                                                          │
│   ┌──────────────┐   ┌──────────────┐                   │
│   │   Service A  │   │   Service B  │                    │
│   │  (hgobj)     │   │  (hgobj)     │                    │
│   │              │   │              │                    │
│   │  ┌────────┐  │   │  ┌────────┐  │                   │
│   │  │Child 1 │  │   │  │Child 2 │  │                   │
│   │  └────────┘  │   │  └────────┘  │                   │
│   └──────────────┘   └──────────────┘                   │
└──────────────────────────────────────────────────────────┘
         │ events (json_t *kw)  ↕  pub/sub
```

Every component is a **GObject** (`hgobj` — an opaque handle) — an instance of a **GClass**. GClasses define:
- Typed **attributes** (schema via `SDATA`)
- A **Finite State Machine** (states + event-action table)
- **Lifecycle methods** (`mt_create`, `mt_start`, `mt_stop`, `mt_destroy`, …)

GObjects communicate exclusively via **events** carrying `json_t *kw` (jansson JSON) payloads. There is no direct function calling between components — everything goes through the FSM event dispatcher.

---

## Build

See the top-level [CLAUDE.md](../../CLAUDE.md) for full build instructions. Summary:

```bash
# 1. Install system dependencies (once)
sudo apt install -y kconfig-frontends ...    # see CLAUDE.md

# 2. Configure
menuconfig      # edit .config: compiler, build type, optional modules

# 3. Configure, build and install external libraries (once, or after compiler change)
./set_compiler.sh
cd kernel/c/linux-ext-libs
./extrae.sh && ./configure-libs.sh

# 4. Initialize (first time or after .config change)
yunetas init    # generates yuneta_version.h, yuneta_config.h, cmake build dirs

# 5. Build & install
yunetas build
```

---

## Module Map

Build order (each layer depends on all above it):

```
kernel/c/gobj-c       ← GObject framework, FSM, SData, helpers, logging
kernel/c/libjwt       ← JWT creation & verification
kernel/c/ytls         ← TLS abstraction (OpenSSL / mbed-TLS)
kernel/c/yev_loop     ← async event loop (Linux io_uring)
kernel/c/timeranger2  ← append-only time-series database + TreeDB
kernel/c/root-linux   ← all Linux runtime GClasses (TCP, timers, protocols, …)
kernel/c/root-esp32   ← ESP32 port of runtime GClasses
```

---

## gobj-c — GObject Framework

**Header:** `gobj-c/src/gobj.h`
**Implementation:** `gobj-c/src/gobj.c` (~12,000 LOC)

The heart of Yuneta. Everything else builds on top of this.

> **Legend:** Functions marked with **`[JS]`** are also available in the JavaScript implementation (`kernel/js/yunetas-js7`). Unmarked functions are C-only.

### Core Concepts

A **GClass** is a registered class definition. A **GObject** (`hgobj`) is an opaque handle to an instance of a GClass. GObjects form a parent-child tree rooted at a **Yuno** (the process root).

### GClass Definition Macros

```c
// Declare a GClass symbol (use in headers)
GOBJ_DECLARE_GCLASS(C_MY_CLASS);

// Define a GClass symbol (use in the .c file)
GOBJ_DEFINE_GCLASS(C_MY_CLASS);              // [JS] as gclass_create()

// Declare/define states and events
GOBJ_DECLARE_STATE(ST_IDLE);
GOBJ_DEFINE_STATE(ST_IDLE);
GOBJ_DECLARE_EVENT(EV_CONNECT);
GOBJ_DEFINE_EVENT(EV_CONNECT);

// Private data helpers (in mt_writing callback)
IF_EQ_SET_PRIV(attr_name, priv_field)
ELIF_EQ_SET_PRIV(attr_name, priv_field)
SET_PRIV(attr_name, priv_field)
```

### GClass Management API

```c
// Register a GClass
hgclass gclass_create(                                                      // [JS]
    gclass_name_t gclass_name,
    event_type_t *event_types,
    states_t *states,
    const GMETHODS *gmt,
    const LMETHOD *lmt,
    const sdata_desc_t *attrs_table,
    size_t priv_size,
    const sdata_desc_t *authz_table,
    const sdata_desc_t *command_table,
    const trace_level_t *s_user_trace_level,
    gclass_flag_t gclass_flag
);

// Build FSM incrementally
int gclass_add_state(hgclass gclass, gobj_state_t state_name);
int gclass_add_ev_action(hgclass gclass, gobj_state_t state_name,
    gobj_event_t event, gobj_action_fn action, gobj_state_t next_state);
int gclass_add_event_type(hgclass gclass, gobj_event_t event_name, event_flag_t event_flag);
int gclass_check_fsm(hgclass gclass);

// Query GClasses
hgclass gclass_find_by_name(gclass_name_t gclass_name);                    // [JS]
gclass_name_t gclass_gclass_name(hgclass gclass);
BOOL gclass_has_attr(hgclass gclass, const char *name);
json_t *gclass_gclass_register(void);       // list all registered GClasses
json_t *gclass2json(hgclass gclass);

// Event introspection
event_type_t *gclass_event_type(hgclass gclass, gobj_event_t event);
gobj_event_t gclass_find_public_event(const char *event, BOOL verbose);
event_type_t *gobj_find_event_type(const char *event, event_flag_t event_flag, BOOL verbose);

// Unregister
void gclass_unregister(hgclass hgclass);                                    // [JS]

// Command introspection
const sdata_desc_t *gclass_command_desc(hgclass gclass, const char *name, BOOL verbose);
```

### Attribute Schema (SData)

```c
PRIVATE sdata_desc_t attrs_table[] = {
    SDATA(DTP_STRING,  "url",      SDF_RD,      "",    "Server URL"),       // [JS]
    SDATA(DTP_INTEGER, "timeout",  SDF_RD,       0,    "Timeout ms"),       // [JS]
    SDATA(DTP_BOOLEAN, "active",   SDF_RD,   FALSE,    "Active flag"),      // [JS]
    SDATA(DTP_STRING,  "saved",    SDF_PERSIST, "",    "Persisted value"),   // [JS]
    SDATA(DTP_JSON,    "config",   SDF_RD,    "{}",    "Config dict"),       // [JS]
    SDATA_END()                                                              // [JS]
};
```

**Data types:** (all available in JS via `data_type_t`)

| Type | C equivalent | JS |
|------|-------------|-----|
| `DTP_STRING` | `const char *` | [JS] |
| `DTP_INTEGER` | `json_int_t` | [JS] |
| `DTP_REAL` | `double` | [JS] |
| `DTP_BOOLEAN` | `BOOL` | [JS] |
| `DTP_LIST` | `json_t *` (array) | [JS] |
| `DTP_DICT` | `json_t *` (object) | [JS] |
| `DTP_JSON` | `json_t *` (any) | [JS] |
| `DTP_POINTER` | `void *` | [JS] |

**Attribute flags (`sdata_flag_t`):** (all available in JS via `sdata_flag_t`)

| Flag | Meaning | JS |
|------|---------|-----|
| `SDF_RD` | Readable externally | [JS] |
| `SDF_WR` | Writable externally | [JS] |
| `SDF_PERSIST` | Saved/loaded via persistence callbacks | [JS] |
| `SDF_VOLATIL` | Not included in stats/snapshots | [JS] |
| `SDF_STATS` | Included in stats output | [JS] |
| `SDF_RSTATS` | Resettable stats | [JS] |
| `SDF_PSTATS` | Persistent stats | [JS] |
| `SDF_AUTHZ_R` | Requires read authorization | [JS] |
| `SDF_AUTHZ_W` | Requires write authorization | [JS] |
| `SDF_AUTHZ_X` | Requires execute authorization | [JS] |
| `SDF_REQUIRED` | Must be provided at creation | [JS] |

### GObject Lifecycle API

```c
// Bootstrap the entire framework
int gobj_start_up(                                              // [JS]
    int argc, char *argv[],
    const json_t *jn_global_settings,
    const persistent_attrs_t *persistent_attrs,
    json_function_fn global_command_parser,
    json_function_fn global_statistics_parser,
    authorization_checker_fn global_authorization_checker,
    authentication_parser_fn global_authentication_parser
);

// Shutdown
void gobj_set_shutdown(void);
BOOL gobj_is_shutdowning(void);
void gobj_set_exit_code(int exit_code);
int  gobj_get_exit_code(void);
void gobj_end(void);

// Creation
hgobj gobj_create(                                              // [JS]
    const char *name,
    gclass_name_t gclass_name,
    json_t *kw,           // initial attribute values
    hgobj parent
);
hgobj gobj_create2(const char *name, gclass_name_t gclass_name,                    // [JS]
    json_t *kw, hgobj parent, gobj_flag_t gobj_flag);
hgobj gobj_create_yuno(const char *name, gclass_name_t gclass_name, json_t *kw);   // [JS]
hgobj gobj_create_service(const char *name, gclass_name_t gclass_name,              // [JS]
    json_t *kw, hgobj parent);
hgobj gobj_create_default_service(const char *name, gclass_name_t gclass_name,      // [JS]
    json_t *kw, hgobj parent);
hgobj gobj_create_volatil(const char *name, gclass_name_t gclass_name,              // [JS]
    json_t *kw, hgobj parent);
hgobj gobj_create_pure_child(const char *name, gclass_name_t gclass_name,           // [JS]
    json_t *kw, hgobj parent);
hgobj gobj_create_tree(hgobj parent, const char *tree_config, json_t *json_config_variables);
hgobj gobj_service_factory(const char *name, json_t *jn_service_config);

// Start / Stop
int  gobj_start(hgobj gobj);              // calls mt_start     [JS]
int  gobj_stop(hgobj gobj);               // calls mt_stop      [JS]
int  gobj_start_children(hgobj gobj);                           // [JS]
int  gobj_stop_children(hgobj gobj);                            // [JS]
int  gobj_start_tree(hgobj gobj);                               // [JS]
int  gobj_stop_tree(hgobj gobj);                                // [JS]

// Play / Pause
int  gobj_play(hgobj gobj);               // calls mt_play      [JS]
int  gobj_pause(hgobj gobj);                                    // [JS]

// Enable / Disable
int  gobj_enable(hgobj gobj);
int  gobj_disable(hgobj gobj);

// Destroy
void gobj_destroy(hgobj gobj);                                  // [JS]
void gobj_destroy_children(hgobj gobj);
int  gobj_destroy_named_children(hgobj gobj, const char *name);

// Status queries
BOOL gobj_is_running(hgobj gobj);                               // [JS]
BOOL gobj_is_playing(hgobj gobj);                               // [JS]
BOOL gobj_is_destroying(hgobj gobj);                            // [JS]
BOOL gobj_is_volatil(hgobj gobj);                               // [JS]
int  gobj_set_volatil(hgobj gobj, BOOL set);
BOOL gobj_is_pure_child(hgobj gobj);                            // [JS]
BOOL gobj_is_disabled(hgobj gobj);
BOOL gobj_is_service(hgobj gobj);                               // [JS]
BOOL gobj_is_top_service(hgobj gobj);
BOOL gobj_typeof_gclass(hgobj gobj, const char *gclass_name);
BOOL gobj_typeof_inherited_gclass(hgobj gobj, const char *gclass_name);
```

### Persistence API

```c
int     gobj_load_persistent_attrs(hgobj gobj, json_t *jn_attrs);       // [JS]
int     gobj_save_persistent_attrs(hgobj gobj, json_t *jn_attrs);       // [JS]
int     gobj_remove_persistent_attrs(hgobj gobj, json_t *jn_attrs);     // [JS]
json_t *gobj_list_persistent_attrs(hgobj gobj, json_t *jn_attrs);       // [JS]
```

### State Machine API

```c
BOOL        gobj_change_state(hgobj gobj, gobj_state_t state_name);     // [JS]
gobj_state_t gobj_current_state(hgobj gobj);                            // [JS]
BOOL        gobj_in_this_state(hgobj gobj, gobj_state_t state);
BOOL        gobj_has_state(hgobj gobj, gobj_state_t state);
BOOL        gobj_has_event(hgobj gobj, gobj_event_t event, event_flag_t event_flag);  // [JS]
BOOL        gobj_has_output_event(hgobj gobj, gobj_event_t event, event_flag_t event_flag);  // [JS]
event_type_t *gobj_event_type(hgobj gobj, gobj_event_t event, BOOL include_system_events);
event_type_t *gobj_event_type_by_name(hgobj gobj, const char *event_name);
```

### Attribute Access API

```c
// Generic
json_t *gobj_read_attr(hgobj gobj, const char *path, hgobj src);                    // [JS]
int     gobj_write_attr(hgobj gobj, const char *path, json_t *value, hgobj src);     // [JS]
json_t *gobj_read_attrs(hgobj gobj, sdata_flag_t include_flag, hgobj src);           // [JS]
int     gobj_write_attrs(hgobj gobj, json_t *kw, sdata_flag_t include_flag, hgobj src);  // [JS]
BOOL    gobj_has_attr(hgobj gobj, const char *name);                                 // [JS]
BOOL    gobj_has_bottom_attr(hgobj gobj, const char *name);

// Attribute introspection
const sdata_desc_t *gclass_attr_desc(hgclass gclass, const char *attr, BOOL verbose);
const sdata_desc_t *gobj_attr_desc(hgobj gobj, const char *attr, BOOL verbose);
data_type_t gobj_attr_type(hgobj gobj, const char *name);
BOOL gobj_is_readable_attr(hgobj gobj, const char *name);
BOOL gobj_is_writable_attr(hgobj gobj, const char *name);
json_t *get_attrs_schema(hgobj gobj);

// Typed reads (with bottom inheritance — reads from bottom_gobj if attr not found)
const char *gobj_read_str_attr(hgobj gobj, const char *name);                       // [JS]
json_int_t  gobj_read_integer_attr(hgobj gobj, const char *name);                   // [JS]
double      gobj_read_real_attr(hgobj gobj, const char *name);
BOOL        gobj_read_bool_attr(hgobj gobj, const char *name);                      // [JS]
json_t     *gobj_read_json_attr(hgobj gobj, const char *name);
void       *gobj_read_pointer_attr(hgobj gobj, const char *name);                   // [JS]

// Typed writes
int gobj_write_str_attr(hgobj gobj, const char *name, const char *value);            // [JS]
int gobj_write_strn_attr(hgobj gobj, const char *name, const char *s, size_t len);
int gobj_write_integer_attr(hgobj gobj, const char *name, json_int_t value);         // [JS]
int gobj_write_real_attr(hgobj gobj, const char *name, double value);
int gobj_write_bool_attr(hgobj gobj, const char *name, BOOL value);                  // [JS]
int gobj_write_json_attr(hgobj gobj, const char *name, json_t *value);
int gobj_write_new_json_attr(hgobj gobj, const char *name, json_t *value);  // steals ref
int gobj_write_pointer_attr(hgobj gobj, const char *name, void *value);

// User data (arbitrary per-gobj JSON storage)
json_t *gobj_read_user_data(hgobj gobj, const char *name);
int     gobj_write_user_data(hgobj gobj, const char *name, json_t *value);
json_t *gobj_kw_get_user_data(hgobj gobj, const char *path, json_t *default_value, kw_flag_t flag);

// Stats
int gobj_reset_volatil_attrs(hgobj gobj);
int gobj_reset_rstats_attrs(hgobj gobj);
```

### Stats API

```c
json_int_t gobj_set_stat(hgobj gobj, const char *path, json_int_t value);
json_int_t gobj_incr_stat(hgobj gobj, const char *path, json_int_t value);
json_int_t gobj_decr_stat(hgobj gobj, const char *path, json_int_t value);
json_int_t gobj_get_stat(hgobj gobj, const char *path);
json_t    *gobj_jn_stats(hgobj gobj);
```

### Event System API

```c
// Sending events
int     gobj_send_event(hgobj dst, gobj_event_t event, json_t *kw, hgobj src);      // [JS]
int     gobj_send_event_to_children(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src);
int     gobj_send_event_to_children_tree(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src);
int     gobj_publish_event(hgobj publisher, gobj_event_t event, json_t *kw);         // [JS]

// Subscriptions
json_t *gobj_subscribe_event(                                                        // [JS]
    hgobj publisher,
    gobj_event_t event,
    json_t *kw,           // subscription options (filter, config)
    hgobj subscriber
);
int gobj_unsubscribe_event(hgobj publisher, gobj_event_t event, json_t *kw, hgobj subscriber);  // [JS]
int gobj_unsubscribe_list(hgobj gobj, json_t *dl_subs, BOOL force);                 // [JS]

json_t *gobj_find_subscriptions(hgobj gobj, gobj_event_t event, json_t *kw, hgobj subscriber);  // [JS]
json_t *gobj_list_subscriptions(hgobj gobj, gobj_event_t event, json_t *kw, hgobj subscriber);  // [JS]
json_t *gobj_find_subscribings(hgobj gobj, gobj_event_t event, json_t *kw, hgobj publisher);    // [JS]
json_t *gobj_list_subscribings(hgobj gobj, gobj_event_t event, json_t *kw, hgobj subscriber);

// Commands & Stats
json_t *gobj_command(hgobj gobj, const char *command, json_t *kw, hgobj src);       // [JS]
int     gobj_audit_commands(int (*audit_command_cb)(const char *command,
    json_t *kw, void *user_data), void *user_data);
json_t *gobj_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src);           // [JS]

// Local methods (call gclass-specific lmt function by name)
json_t *gobj_local_method(hgobj gobj, const char *lmethod, json_t *kw, hgobj src);
```

### Hierarchy & Navigation

```c
// Parent / yuno / services
hgobj       gobj_parent(hgobj gobj);                            // [JS]
hgobj       gobj_yuno(void);                                    // [JS]
hgobj       gobj_default_service(void);                         // [JS]
hgobj       gobj_find_service(const char *name, BOOL verbose);  // [JS]
hgobj       gobj_find_service_by_gclass(const char *gclass_name, BOOL verbose);
hgobj       gobj_nearest_top_service(hgobj gobj);
json_t     *gobj_services(void);                                // [JS]
json_t     *gobj_top_services(void);

// Identification
const char *gobj_name(hgobj gobj);                              // [JS]
const char *gobj_short_name(hgobj gobj);                        // [JS]
const char *gobj_full_name(hgobj gobj);                         // [JS]
gclass_name_t gobj_gclass_name(hgobj gobj);                    // [JS]
hgclass     gobj_gclass(hgobj gobj);
void       *gobj_priv_data(hgobj gobj);

// Yuno identification
const char *gobj_yuno_name(void);                               // [JS]
const char *gobj_yuno_role(void);                               // [JS]
const char *gobj_yuno_id(void);                                 // [JS]
const char *gobj_yuno_tag(void);
const char *gobj_yuno_role_plus_name(void);
const char *gobj_yuno_realm_id(void);
const char *gobj_yuno_realm_owner(void);
const char *gobj_yuno_realm_role(void);
const char *gobj_yuno_realm_name(void);
const char *gobj_yuno_realm_env(void);
const char *gobj_yuno_node_owner(void);
json_t     *gobj_global_variables(void);

// Child iteration
hgobj       gobj_first_child(hgobj gobj);
hgobj       gobj_last_child(hgobj gobj);
hgobj       gobj_next_child(hgobj child);
hgobj       gobj_prev_child(hgobj child);
hgobj       gobj_child_by_name(hgobj gobj, const char *name);
hgobj       gobj_child_by_index(hgobj gobj, size_t index);
size_t      gobj_child_size(hgobj gobj);
size_t      gobj_child_size2(hgobj gobj, json_t *jn_filter);

// Find / search
hgobj       gobj_find_child(hgobj gobj, json_t *jn_filter);     // [JS]
hgobj       gobj_find_child_by_tree(hgobj gobj, json_t *jn_filter);
hgobj       gobj_find_gobj(const char *gobj_path);              // [JS]
hgobj       gobj_search_path(hgobj gobj, const char *path);     // [JS]
BOOL        gobj_match_gobj(hgobj gobj, json_t *jn_filter);     // [JS]
json_t     *gobj_match_children(hgobj gobj, json_t *jn_filter); // [JS]
json_t     *gobj_match_children_tree(hgobj gobj, json_t *jn_filter);  // [JS]
int         gobj_free_iter(json_t *iter);

// Walk (callback-based iteration)
int gobj_walk_gobj_children(hgobj gobj, walk_type_t walk_type,              // [JS]
    cb_walking_t cb, void *user_data, void *user_data2, void *user_data3);
int gobj_walk_gobj_children_tree(hgobj gobj, walk_type_t walk_type,         // [JS]
    cb_walking_t cb, void *user_data, void *user_data2, void *user_data3);

// Bottom gobj (protocol stack chaining)
hgobj gobj_set_bottom_gobj(hgobj gobj, hgobj bottom_gobj);     // [JS]
hgobj gobj_bottom_gobj(hgobj gobj);                             // [JS]
hgobj gobj_last_bottom_gobj(hgobj gobj);
BOOL  gobj_is_bottom_gobj(hgobj gobj);

// Re-parent
int gobj_change_parent(hgobj gobj, hgobj gobj_new_parent);     // [JS]

// Introspection
json_t *gobj2json(hgobj gobj, json_t *jn_filter);
json_t *gobj_view_tree(hgobj gobj, json_t *jn_filter);
const sdata_desc_t *gobj_command_desc(hgobj gobj, const char *name, BOOL verbose);
```

### Authorization

```c
// Register authz checker (called at gobj_start_up)
typedef BOOL (*authz_checker_fn)(hgobj gobj, const char *authz,
    json_t *kw, hgobj src);

// Authorization checking
BOOL    gobj_user_has_authz(hgobj gobj, const char *authz, json_t *kw, hgobj src);
json_t *gobj_authenticate(hgobj gobj, json_t *kw, hgobj src);
json_t *gobj_authz(hgobj gobj, const char *authz);
json_t *gobj_authzs(hgobj gobj);

// Authz introspection
const sdata_desc_t *gobj_get_global_authz_table(void);
const sdata_desc_t *gclass_authz_desc(hgclass gclass);
json_t *authzs_list(hgobj gobj, const char *authz);
const sdata_desc_t *authz_get_level_desc(const sdata_desc_t *authz_table, const char *authz);
json_t *gobj_build_authzs_doc(hgobj gobj, const char *cmd, json_t *kw);
```

**Built-in authz types:** `__read_attribute__`, `__write_attribute__`, `__execute_command__`, `__inject_event__`, `__subscribe_event__`, `__read_stats__`, `__reset_stats__`

### Tracing & Debugging

```c
// Global trace levels (OR-combined bitmask)
#define TRACE_MACHINE           0x00000001  // FSM event dispatch
#define TRACE_CREATE_DELETE     0x00000002  // GObject create/destroy
#define TRACE_SUBSCRIPTIONS     0x00000004  // Subscribe/unsubscribe
#define TRACE_START_STOP        0x00000008  // Start/stop
#define TRACE_EV_KW             0x00000010  // Event kw content
#define TRACE_STATES            0x00000020  // State transitions
#define TRACE_GBUFFERS          0x00000040  // Buffer ops
#define TRACE_TIMER             0x00000080  // Timer events

// Set trace levels
int gobj_set_global_trace(const char *level, BOOL set);
int gobj_set_global_no_trace(const char *level, BOOL set);
int gobj_set_gclass_trace(hgclass gclass, const char *level, BOOL set);
int gobj_set_gclass_no_trace(hgclass gclass, const char *level, BOOL set);
int gobj_set_gobj_trace(hgobj gobj, const char *level, BOOL set, json_t *kw);
int gobj_set_gobj_no_trace(hgobj gobj, const char *level, BOOL set);

// Query trace levels
uint32_t gobj_trace_level(hgobj gobj);
uint32_t gobj_trace_no_level(hgobj gobj);
uint32_t gobj_global_trace_level(void);
BOOL     gobj_is_level_tracing(hgobj gobj, uint32_t level);
BOOL     gobj_is_level_not_tracing(hgobj gobj, uint32_t level);

// Trace level introspection
json_t *gobj_repr_global_trace_levels(void);
json_t *gobj_repr_gclass_trace_levels(const char *gclass_name);
json_t *gobj_trace_level_list(hgclass gclass);
json_t *gobj_get_global_trace_level(void);
json_t *gobj_get_gclass_trace_level(hgclass gclass);
json_t *gobj_get_gclass_trace_no_level(hgclass gclass);
json_t *gobj_get_gobj_trace_level(hgobj gobj);
json_t *gobj_get_gobj_trace_no_level(hgobj gobj);
json_t *gobj_get_gobj_trace_level_tree(hgobj gobj);

// Trace filters (per-GClass filtering by attribute value)
int     gobj_load_trace_filter(hgclass gclass, json_t *jn_trace_filter);
int     gobj_add_trace_filter(hgclass gclass, const char *attr, const char *value);
int     gobj_remove_trace_filter(hgclass gclass, const char *attr, const char *value);
json_t *gobj_get_trace_filter(hgclass gclass);

// Deep tracing (verbose debugging)
int gobj_set_deep_tracing(int level);
int gobj_get_deep_tracing(void);
```

### Helpers & Utilities

All in `gobj-c/src/helpers.h` and `kwid.h`.

**Keyword (kw) operations:**
```c
// Flags
#define KW_REQUIRED     0x0001                                                       // [JS]
#define KW_CREATE       0x0002                                                       // [JS]
#define KW_EXTRACT      0x0004                                                       // [JS]
#define KW_WILD_NUMBER  0x0008                                                       // [JS]

// Typed getters (with default and flags)
BOOL        kw_get_bool(hgobj gobj, json_t *kw, const char *key, BOOL def, kw_flag_t flag);     // [JS]
json_int_t  kw_get_int(hgobj gobj, json_t *kw, const char *key, json_int_t def, kw_flag_t flag);  // [JS]
double      kw_get_real(hgobj gobj, json_t *kw, const char *key, double def, kw_flag_t flag);   // [JS]
const char *kw_get_str(hgobj gobj, json_t *kw, const char *key, const char *def, kw_flag_t flag);  // [JS]
json_t     *kw_get_dict(hgobj gobj, json_t *kw, const char *key, json_t *def, kw_flag_t flag);  // [JS]
json_t     *kw_get_list(hgobj gobj, json_t *kw, const char *key, json_t *def, kw_flag_t flag);  // [JS]
json_t     *kw_get_dict_value(hgobj gobj, json_t *kw, const char *path,             // [JS]
    json_t *default_value, kw_flag_t flag);
json_t     *kw_get_subdict_value(hgobj gobj, json_t *kw, const char *path,
    const char *key, json_t *default_value, kw_flag_t flag);
json_t     *kw_get_list_value(hgobj gobj, json_t *kw, int idx, kw_flag_t flag);

// kw manipulation
json_t *kw_duplicate(hgobj gobj, json_t *kw);
json_t *kw_incref(json_t *kw);
json_t *kw_decref(json_t *kw);
BOOL    kw_has_key(json_t *kw, const char *key);                                    // [JS]
int     kw_delete(hgobj gobj, json_t *kw, const char *path);                        // [JS]
int     kw_delete_subkey(hgobj gobj, json_t *kw, const char *path, const char *key);
json_t *kw_find_path(hgobj gobj, json_t *kw, const char *path, BOOL verbose);       // [JS]
int     kw_pop(json_t *kw1, json_t *kw2);                                           // [JS]

// Matching & filtering
BOOL    kw_match_simple(json_t *kw, json_t *kw_filter);                             // [JS]
json_t *kw_select(hgobj gobj, json_t *kw, json_t *jn_filter,                        // [JS]
    BOOL (*match_fn)(json_t *kw, json_t *jn_filter));
json_t *kw_collect(hgobj gobj, json_t *kw, json_t *jn_filter,                       // [JS]
    BOOL (*match_fn)(json_t *kw, json_t *jn_filter));
json_t *kw_clone_by_keys(hgobj gobj, json_t *kw, json_t *keys, BOOL verbose);       // [JS]
json_t *kw_clone_by_not_keys(hgobj gobj, json_t *kw, json_t *keys, BOOL verbose);   // [JS]
json_t *kw_clone_by_path(hgobj gobj, json_t *kw, const char **paths);
void    kw_update_except(hgobj gobj, json_t *kw, json_t *other, const char **except_keys);
int     kw_delete_private_keys(json_t *kw);
int     kw_delete_metadata_keys(json_t *kw);
json_t *kw_filter_private(hgobj gobj, json_t *kw);
json_t *kw_filter_metadata(hgobj gobj, json_t *kw);

// Set values (dot-separated path)
int kw_set_dict_value(hgobj gobj, json_t *kw, const char *path, json_t *value);     // [JS]
int kw_set_subdict_value(hgobj gobj, json_t *kw,                                    // [JS]
    const char *subdict, const char *key, json_t *value);

// KWID (keyword ID) operations
json_t *kwid_get(hgobj gobj, json_t *kw, kw_flag_t flag, const char *path, ...);
json_t *kwid_new_list(hgobj gobj, json_t *kw, kw_flag_t flag, const char *path, ...);
json_t *kwid_new_dict(hgobj gobj, json_t *kw, kw_flag_t flag, const char *path, ...);  // [JS]
json_t *kwid_get_ids(json_t *ids);                                                   // [JS]
BOOL    kwid_match_id(hgobj gobj, json_t *ids, const char *id);                      // [JS]
BOOL    kwid_match_nid(hgobj gobj, json_t *ids, const char *id, int max_id_size);
BOOL    kwid_compare_records(hgobj gobj, json_t *record, json_t *expected,
    BOOL without_metadata, BOOL without_private, BOOL verbose);
BOOL    kwid_compare_lists(hgobj gobj, json_t *list, json_t *expected,
    BOOL without_metadata, BOOL without_private, BOOL verbose);

// Serialization
json_t *kw_serialize(hgobj gobj, json_t *kw);
json_t *kw_deserialize(hgobj gobj, json_t *kw);
```

**String utilities:**
```c
BOOL empty_string(const char *s);                                                    // [JS]
BOOL str_in_list(const char **list, const char *str, BOOL ignore_case);              // [JS]
int  idx_in_list(const char **list, const char *str, BOOL ignore_case);
char *build_path(char *bf, size_t bfsize, ...);
char *get_last_segment(char *path);
char *pop_last_segment(char *path);
const char **split2(const char *str, const char *delim, int *list_size);
void split_free2(const char **list);
```

**JSON helpers:**
```c
json_t *string2json(const char *str, BOOL verbose);
json_t *anystring2json(const char *bf, size_t len, BOOL verbose);
char   *json2str(const json_t *jn);
char   *json2uglystr(const json_t *jn);
size_t  json_size(json_t *value);
BOOL    json_is_identical(json_t *kw1, json_t *kw2);                                // [JS]
int     cmp_two_simple_json(json_t *jn_var1, json_t *jn_var2);                       // [JS]
int     json_dict_recursive_update(json_t *object, json_t *other, BOOL overwrite);
json_t *json_replace_var(json_t *jn_dict, json_t *jn_vars);

// Type conversion
double      jn2real(json_t *jn_var);
json_int_t  jn2integer(json_t *jn_var);
char       *jn2string(json_t *jn_var);
BOOL        jn2bool(json_t *jn_var);

// JSON list utilities
int    json_list_str_index(json_t *jn_list, const char *str, BOOL ignore_case);
int    json_list_find(json_t *list, json_t *value);
int    json_list_update(json_t *list, json_t *other, BOOL as_set_type);
BOOL   json_str_in_list(hgobj gobj, json_t *jn_list, const char *str, BOOL ignore_case);

// JSON records from descriptor
json_t *create_json_record(hgobj gobj, const json_desc_t *json_desc);               // [JS]

// JSON file I/O
json_t *load_json_from_file(hgobj gobj, const char *directory,
    const char *filename, log_opt_t on_critical_error);
int save_json_to_file(hgobj gobj, const char *directory,
    const char *filename, int xpermission, int rpermission,
    log_opt_t on_critical_error, BOOL create, BOOL only_read, json_t *jn_data);
json_t *load_persistent_json(hgobj gobj, const char *directory,
    const char *filename, log_opt_t on_critical_error,
    int *pfd, BOOL exclusive, BOOL silence);

// JSON refcount debugging
int json_check_refcounts(json_t *kw, int max_refcount, int *result);
```

**File system utilities:**
```c
int  newdir(const char *path, int xpermission);
int  newfile(const char *path, int rpermission, BOOL overwrite);
int  mkrdir(const char *path, int xpermission);
int  rmrdir(const char *root_dir);
int  rmrcontentdir(const char *root_dir);
BOOL is_regular_file(const char *path);
BOOL is_directory(const char *path);
off_t file_size(const char *path);
BOOL file_exists(const char *directory, const char *filename);
BOOL subdir_exists(const char *directory, const char *subdir);
int  file_remove(const char *directory, const char *filename);
int  copyfile(const char *source, const char *destination, int permission, BOOL overwrite);
```

**URL parsing:**
```c
int parse_url(hgobj gobj, const char *uri,
    char *schema, size_t schema_size,
    char *host, size_t host_size,
    char *port, size_t port_size,
    char *path, size_t path_size,
    char *query, size_t query_size,
    BOOL no_schema);
```

**Time utilities:**
```c
char    *current_timestamp(char *bf, size_t bfsize);
time_t   start_sectimer(time_t seconds);
BOOL     test_sectimer(time_t value);
uint64_t start_msectimer(uint64_t milliseconds);
BOOL     test_msectimer(uint64_t value);
uint64_t time_in_milliseconds(void);
uint64_t time_in_milliseconds_monotonic(void);
time_t   time_in_seconds(void);
char    *formatdate(time_t t, char *bf, int bfsize, const char *format);
```

**Metadata & private key detection:**
```c
BOOL is_metadata_key(const char *key);                                               // [JS]
BOOL is_private_key(const char *key);                                                // [JS]
```

**Miscellaneous:**
```c
const char *node_uuid(void);                                                         // [JS]
const char *get_hostname(void);
int create_random_uuid(char *bf, int bfsize);
char *hex2bin(char *bf, int bfsize, const char *hex, size_t hex_len, size_t *out_len);
char *bin2hex(char *bf, int bfsize, const uint8_t *bin, size_t bin_len);
void nice_size(char *bf, size_t bfsize, uint64_t bytes, BOOL b1024);

// Protocol registration
int comm_prot_register(gclass_name_t gclass_name, const char *schema);
gclass_name_t comm_prot_get_gclass(const char *schema);
void comm_prot_free(void);

// Directory walking
int walk_dir_tree(hgobj gobj, const char *root_dir, const char *pattern,
    wd_option opt, walkdir_cb cb, void *user_data);
```

**Inter-event message stack** (from `msg_ievent.h` in root-linux):
```c
// IEvent creation
json_t    *iev_create(hgobj gobj, gobj_event_t event, json_t *kw);
json_t    *iev_create2(hgobj gobj, gobj_event_t event, json_t *kw, json_t *kw_request);
gbuffer_t *iev_create_to_gbuffer(hgobj gobj, gobj_event_t event, json_t *kw);
json_t    *iev_create_from_gbuffer(hgobj gobj, const char **event, gbuffer_t *gbuf, int verbose);

// Message metadata stack
int     msg_iev_push_stack(hgobj gobj, json_t *kw, const char *stack, json_t *jn_data);  // [JS]
json_t *msg_iev_get_stack(hgobj gobj, json_t *kw, const char *stack, BOOL verbose);      // [JS]
json_t *msg_iev_pop_stack(hgobj gobj, json_t *kw, const char *stack);

// Message type
int         msg_iev_set_msg_type(hgobj gobj, json_t *kw, const char *msg_type);     // [JS]
const char *msg_iev_get_msg_type(hgobj gobj, json_t *kw);                           // [JS]

// Response building
json_t *msg_iev_set_back_metadata(hgobj gobj, json_t *kw_request,
    json_t *kw_response, BOOL reverse_dst);
json_t *msg_iev_build_response(hgobj gobj, json_int_t result,
    json_t *jn_comment, json_t *jn_schema, json_t *jn_data, json_t *kw_request);
json_t *msg_iev_clean_metadata(json_t *kw);
```

### Logging

All in `gobj-c/src/glogger.h`.

```c
// Core log functions (variadic key-value pairs terminated by NULL)
void gobj_log_alert(hgobj gobj, log_opt_t opt, ...);
void gobj_log_critical(hgobj gobj, log_opt_t opt, ...);
void gobj_log_error(hgobj gobj, log_opt_t opt, ...);                // [JS] as log_error()
void gobj_log_warning(hgobj gobj, log_opt_t opt, ...);              // [JS] as log_warning()
void gobj_log_info(hgobj gobj, log_opt_t opt, ...);                 // [JS] as log_info()
void gobj_log_debug(hgobj gobj, log_opt_t opt, ...);                // [JS] as log_debug()

// Usage example (key-value pairs terminated by NULL)
gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
    "function",     __FUNCTION__,
    "msgset",       MSGSET_PARAMETER_ERROR,
    "msg",          "Invalid parameter",
    "key",          key,
    NULL
);

// Trace output
void gobj_trace_msg(hgobj gobj, const char *fmt, ...);              // [JS] as trace_msg()
void gobj_trace_json(hgobj gobj, json_t *jn, const char *fmt, ...); // [JS] as trace_json()
void gobj_trace_dump(hgobj gobj, const char *bf, size_t len, const char *fmt, ...);
void gobj_trace_dump_gbuf(hgobj gobj, gbuffer_t *gbuf, const char *fmt, ...);
void gobj_info_msg(hgobj gobj, const char *fmt, ...);
int  trace_msg0(const char *fmt, ...);  // without gobj context

// Log handler management
int  gobj_log_register_handler(const char *handler_type,
    loghandler_close_fn_t close_fn, loghandler_write_fn_t write_fn,
    loghandler_fwrite_fn_t fwrite_fn);
int  gobj_log_add_handler(const char *handler_name, const char *handler_type,
    log_handler_opt_t handler_options, void *h);
int  gobj_log_del_handler(const char *handler_name);
BOOL gobj_log_exist_handler(const char *handler_name);
json_t *gobj_log_list_handlers(void);

// Log inspection
json_t     *gobj_get_log_data(void);
const char *gobj_log_last_message(void);
void        gobj_log_clear_counters(void);
void        gobj_log_clear_log_file(void);

// Backtrace
void set_show_backtrace_fn(show_backtrace_fn_t show_backtrace_fn);
void print_backtrace(void);
```

### GBuffer — Growable Buffer

**Header:** `gobj-c/src/gbuffer.h`

Growable byte buffer used throughout Yuneta for I/O, serialization, and protocol framing.

```c
// Lifecycle
gbuffer_t *gbuffer_create(size_t data_size, size_t max_memory_size);
void       gbuffer_remove(gbuffer_t *gbuf);

// Write data
size_t gbuffer_append(gbuffer_t *gbuf, void *data, size_t len);
size_t gbuffer_append_json(gbuffer_t *gbuf, json_t *jn);
int    gbuffer_append_gbuf(gbuffer_t *dst, gbuffer_t *src);
int    gbuffer_printf(gbuffer_t *gbuf, const char *format, ...);
int    gbuffer_vprintf(gbuffer_t *gbuf, const char *format, va_list ap);

// Read data
void *gbuffer_get(gbuffer_t *gbuf, size_t len);
char *gbuffer_getline(gbuffer_t *gbuf, char separator);
int   gbuffer_set_rd_offset(gbuffer_t *gbuf, size_t position);
int   gbuffer_set_wr(gbuffer_t *gbuf, size_t offset);

// Label
int gbuffer_setlabel(gbuffer_t *gbuf, const char *label);

// Conversions
gbuffer_t *str2gbuf(const char *fmt, ...);
gbuffer_t *json2gbuf(gbuffer_t *gbuf, json_t *jn, size_t flags);
json_t    *gbuf2json(gbuffer_t *gbuf, int verbose);
int        gbuf2file(hgobj gobj, gbuffer_t *gbuf, const char *path, int permission, BOOL overwrite);

// Base64
gbuffer_t *gbuffer_binary_to_base64(const char *src, size_t len);
gbuffer_t *gbuffer_base64_to_binary(const char *base64, size_t base64_len);
gbuffer_t *gbuffer_encode_base64(gbuffer_t *gbuf_input);
gbuffer_t *gbuffer_file2base64(const char *path);

// Serialization
json_t    *gbuffer_serialize(hgobj gobj, gbuffer_t *gbuf);
gbuffer_t *gbuffer_deserialize(hgobj gobj, const json_t *jn);

// Debug
void gobj_trace_dump_gbuf(hgobj gobj, gbuffer_t *gbuf, const char *fmt, ...);
void gobj_trace_dump_full_gbuf(hgobj gobj, gbuffer_t *gbuf, const char *fmt, ...);
```

### Resource API

GObjects that implement `mt_create_resource` / `mt_save_resource` etc. expose CRUD operations:

```c
json_t *gobj_create_resource(hgobj gobj, const char *resource, json_t *kw, json_t *jn_options);
int     gobj_save_resource(hgobj gobj, const char *resource, json_t *record, json_t *jn_options);
int     gobj_delete_resource(hgobj gobj, const char *resource, json_t *record, json_t *jn_options);
json_t *gobj_list_resource(hgobj gobj, const char *resource, json_t *jn_filter, json_t *jn_options);
json_t *gobj_get_resource(hgobj gobj, const char *resource, json_t *jn_filter, json_t *jn_options);
```

### Node API (TreeDB operations via GObj)

GObjects that implement TreeDB methods (Linux only):

```c
json_t *gobj_treedbs(hgobj gobj, json_t *kw, hgobj src);
json_t *gobj_treedb_topics(hgobj gobj, const char *treedb_name, json_t *options, hgobj src);
json_t *gobj_topic_desc(hgobj gobj, const char *topic_name);
size_t  gobj_topic_size(hgobj gobj, const char *topic_name, const char *key);

// Node CRUD
json_t *gobj_create_node(hgobj gobj, const char *topic_name, json_t *kw, json_t *jn_options, hgobj src);
json_t *gobj_update_node(hgobj gobj, const char *topic_name, json_t *kw, json_t *jn_options, hgobj src);
int     gobj_delete_node(hgobj gobj, const char *topic_name, json_t *kw, json_t *jn_options, hgobj src);
json_t *gobj_get_node(hgobj gobj, const char *topic_name, json_t *kw, json_t *jn_options, hgobj src);
json_t *gobj_list_nodes(hgobj gobj, const char *topic_name, json_t *jn_filter, json_t *jn_options, hgobj src);
json_t *gobj_list_instances(hgobj gobj, const char *topic_name,
    const char *pkey2_field, json_t *jn_filter, json_t *jn_options, hgobj src);

// Node relationships
int     gobj_link_nodes(hgobj gobj, const char *hook, const char *parent_topic_name,
    json_t *parent_record, const char *child_topic_name, json_t *child_record, hgobj src);
int     gobj_unlink_nodes(hgobj gobj, const char *hook, const char *parent_topic_name,
    json_t *parent_record, const char *child_topic_name, json_t *child_record, hgobj src);
json_t *gobj_node_parents(hgobj gobj, const char *topic_name, json_t *kw,
    const char *link, json_t *jn_options, hgobj src);
json_t *gobj_node_children(hgobj gobj, const char *topic_name, json_t *kw,
    const char *hook, json_t *jn_filter, json_t *jn_options, hgobj src);
json_t *gobj_node_tree(hgobj gobj, const char *topic_name, json_t *kw, json_t *jn_options, hgobj src);
json_t *gobj_topic_jtree(hgobj gobj, const char *topic_name, const char *hook,
    const char *rename_hook, json_t *kw, json_t *jn_filter, json_t *jn_options, hgobj src);

// Snapshots
int     gobj_shoot_snap(hgobj gobj, const char *tag, json_t *kw, hgobj src);
int     gobj_activate_snap(hgobj gobj, const char *tag, json_t *kw, hgobj src);
json_t *gobj_list_snaps(hgobj gobj, json_t *filter, hgobj src);
```

### Memory Management (gbmem)

**Header:** `gobj-c/src/gbmem.h`

```c
int   gbmem_setup(size_t mem_max_block, size_t mem_max_system_memory,
    BOOL use_own_system_memory, size_t mem_min_block, size_t mem_superblock);
void  gbmem_shutdown(void);
void *gbmem_malloc(size_t size);
void  gbmem_free(void *ptr);
void *gbmem_realloc(void *ptr, size_t size);
void *gbmem_calloc(size_t n, size_t size);
char *gbmem_strdup(const char *str);
char *gbmem_strndup(const char *str, size_t size);
```

### Rotatory Log Files

**Header:** `gobj-c/src/rotatory.h`

```c
int        rotatory_start_up(void);
void       rotatory_end(void);
hrotatory_h rotatory_open(const char *path, size_t bf_size,
    size_t max_megas_rotatoryfile_size, size_t min_free_disk_percentage,
    int xpermission, int rpermission, BOOL exit_on_fail);
void       rotatory_close(hrotatory_h hr);
int        rotatory_write(hrotatory_h hr, int priority, const char *bf, size_t len);
int        rotatory_fwrite(hrotatory_h hr, int priority, const char *format, ...);
void       rotatory_trunk(hrotatory_h hr);
void       rotatory_flush(hrotatory_h hr);
const char *rotatory_path(hrotatory_h hr);
```

### Testing Utilities

**Header:** `gobj-c/src/testing.h`

```c
void set_expected_results(const char *name, json_t *errors_list, json_t *expected,
    const char **ignore_keys, BOOL verbose);
int  test_json(json_t *jn_found);
int  test_json_file(const char *file);
int  test_list(json_t *found, json_t *expected, const char *msg, ...);
int  test_directory_permission(const char *path, mode_t permission);
int  test_file_permission_and_size(const char *path, mode_t permission, off_t size);
int  capture_log_write(void *v, int priority, const char *bf, size_t len);
```

---

## yev_loop — Async Event Loop

**Header:** `yev_loop/src/yev_loop.h`
**Implementation:** `yev_loop/src/yev_loop.c`

Single-threaded, non-blocking I/O engine based on **Linux io_uring**. All I/O, timers, and network events are driven from a single loop with zero blocking. Scaling is achieved by running one yuno per CPU core with inter-yuno messaging.

### Loop Management

```c
int  yev_loop_create(hgobj yuno, unsigned entries, int keep_alive,
    yev_callback_t callback, yev_loop_h *yev_loop);
void yev_loop_destroy(yev_loop_h yev_loop);
int  yev_loop_run(yev_loop_h yev_loop, int timeout_in_seconds);
int  yev_loop_run_once(yev_loop_h yev_loop);
int  yev_loop_stop(yev_loop_h yev_loop);
void yev_loop_reset_running(yev_loop_h yev_loop);
hgobj yev_get_yuno(yev_loop_h yev_loop);
```

### Event Types

```c
typedef enum {
    YEV_CONNECT_TYPE,   // Outgoing TCP connection established/failed
    YEV_ACCEPT_TYPE,    // Incoming TCP connection accepted
    YEV_READ_TYPE,      // Data available to read
    YEV_WRITE_TYPE,     // Write completed
    YEV_TIMER_TYPE,     // Timer fired
    YEV_POLL_TYPE,      // File descriptor poll
    YEV_RECVMSG_TYPE,   // UDP receive
    YEV_SENDMSG_TYPE,   // UDP send
} yev_type_t;
```

### Event Lifecycle

```c
// Create events — timers
yev_event_h yev_create_timer_event(yev_loop_h loop, yev_callback_t cb, hgobj gobj);

// Create events — TCP connect/accept
yev_event_h yev_create_connect_event(yev_loop_h loop, yev_callback_t cb,
    const char *dst_url, const char *src_url, int ai_family, int ai_flags, hgobj gobj);
int yev_rearm_connect_event(yev_event_h ev, const char *dst_url,
    const char *src_url, int ai_family, int ai_flags);
yev_event_h yev_create_accept_event(yev_loop_h loop, yev_callback_t cb,
    const char *listen_url, int backlog, BOOL shared, int ai_family, int ai_flags, hgobj gobj);
yev_event_h yev_dup_accept_event(yev_event_h yev_server_accept, int dup_idx, hgobj gobj);
yev_event_h yev_dup2_accept_event(yev_loop_h loop, yev_callback_t cb, int fd_listen, hgobj gobj);

// Create events — fd read/write
yev_event_h yev_create_read_event(yev_loop_h loop, yev_callback_t cb,
    hgobj gobj, int fd, gbuffer_t *gbuf);
yev_event_h yev_create_write_event(yev_loop_h loop, yev_callback_t cb,
    hgobj gobj, int fd, gbuffer_t *gbuf);
yev_event_h yev_create_poll_event(yev_loop_h loop, yev_callback_t cb,
    hgobj gobj, int fd, unsigned poll_mask);

// Create events — UDP
yev_event_h yev_create_recvmsg_event(yev_loop_h loop, yev_callback_t cb,
    hgobj gobj, int fd, gbuffer_t *gbuf);
yev_event_h yev_create_sendmsg_event(yev_loop_h loop, yev_callback_t cb,
    hgobj gobj, int fd, gbuffer_t *gbuf, struct sockaddr *dst_addr);

// Arm / Disarm
int  yev_start_event(yev_event_h ev);
int  yev_start_timer_event(yev_event_h ev, time_t timeout_ms, BOOL periodic);
int  yev_stop_event(yev_event_h ev);          // idempotent
void yev_destroy_event(yev_event_h ev);

// Mutation
int yev_set_gbuffer(yev_event_h ev, gbuffer_t *gbuf);

// Inspection
const char  *yev_get_state_name(yev_event_h ev);
const char  *yev_event_type_name(yev_event_h ev);
const char **yev_flag_strings(void);
```

### Callback Signature

```c
typedef int (*yev_callback_t)(yev_event_t *event);

// Inside callback:
int my_callback(yev_event_t *ev) {
    hgobj gobj = yev_get_gobj(ev);
    int result = yev_get_result(ev);     // negative = error (errno)
    gbuffer_t *gbuf = yev_get_gbuf(ev);

    if(result < 0) {
        // error or EOF
        return -1;
    }
    // process gbuf data...
    return 0;
}
```

---

## timeranger2 — Time-Series Database

**Header:** `timeranger2/src/timeranger2.h`
**Implementation:** `timeranger2/src/timeranger2.c`

Append-only log database with key-indexed access. Used for MQTT session persistence, message queues, TreeDB, and snapshotting.

### Disk Layout

```
{path}/{database}/
├── __timeranger2__.json     ← database metadata
└── {topic}/
    ├── topic_desc.json      ← immutable topic definition
    ├── topic_cols.json      ← field definitions (optional)
    ├── topic_var.json       ← variable metadata
    └── keys/
        └── {key}/
            ├── {date}.json  ← data records (append-only)
            └── {date}.md2   ← metadata records
```

### Startup

```c
json_t *tranger2_startup(hgobj gobj, json_t *jn_tranger, yev_loop_h yev_loop);
int     tranger2_stop(json_t *tranger);
int     tranger2_shutdown(json_t *tranger);
```

Key config fields: `path` (required), `database`, `master` (BOOL — enables write), `filename_mask` (strftime format), `xpermission`, `rpermission`

### Topic Management

```c
json_t *tranger2_create_topic(json_t *tranger, const char *topic_name,
    const char *pkey, const char *tkey, json_t *jn_topic_ext,
    system_flag2_t system_flag, json_t *jn_cols, json_t *jn_var);

json_t *tranger2_open_topic(json_t *tranger, const char *topic_name, BOOL verbose);
json_t *tranger2_topic(json_t *tranger, const char *topic_name);  // open if needed
int     tranger2_close_topic(json_t *tranger, const char *topic_name);
int     tranger2_delete_topic(json_t *tranger, const char *topic_name);

json_t     *tranger2_list_topics(json_t *tranger);
json_t     *tranger2_list_topic_names(json_t *tranger);
int         tranger2_topic_path(char *bf, size_t bfsize, json_t *tranger, const char *topic_name);
const char *tranger2_topic_name(json_t *topic);
json_t     *tranger2_list_keys(json_t *tranger, const char *topic_name);
uint64_t    tranger2_topic_size(json_t *tranger, const char *topic_name);
uint64_t    tranger2_topic_key_size(json_t *tranger, const char *topic_name, const char *key);

// Topic metadata
int     tranger2_write_topic_var(json_t *tranger, const char *topic_name, json_t *jn_topic_var);
int     tranger2_write_topic_cols(json_t *tranger, const char *topic_name, json_t *jn_cols);
json_t *tranger2_topic_desc(json_t *tranger, const char *topic_name);
json_t *tranger2_list_topic_desc_cols(json_t *tranger, const char *topic_name);
json_t *tranger2_dict_topic_desc_cols(json_t *tranger, const char *topic_name);

// Backup
json_t *tranger2_backup_topic(json_t *tranger, const char *topic_name,
    const char *backup_path, const char *backup_name, BOOL overwrite_backup,
    tranger_backup_deleting_callback_t cb);

// System flags
system_flag2_t tranger2_str2system_flag(const char *system_flag);
void tranger2_set_trace_level(json_t *tranger, int trace_level);
```

### Writing Records

```c
int tranger2_append_record(
    json_t *tranger,
    const char *topic_name,
    uint64_t __t__,             // timestamp (0 = now)
    uint16_t user_flag,
    md2_record_ex_t *md_record_ex,  // out: metadata
    json_t *jn_record               // data to write (owned)
);

int      tranger2_delete_record(json_t *tranger, const char *topic_name, const char *key);
int      tranger2_write_user_flag(json_t *tranger, const char *topic_name,
    const char *key, uint64_t __t__, uint64_t rowid, uint16_t user_flag);
int      tranger2_set_user_flag(json_t *tranger, const char *topic_name,
    const char *key, uint64_t __t__, uint64_t rowid, uint16_t mask, BOOL set);
uint16_t tranger2_read_user_flag(json_t *tranger, const char *topic_name,
    const char *key, uint64_t __t__, uint64_t rowid);
json_t  *tranger2_read_record_content(json_t *tranger, json_t *topic,
    const char *key, md2_record_ex_t *md_record_ex);
```

### Reading: Iterators

For historical bulk reads over a key's records:

```c
json_t *tranger2_open_iterator(
    json_t *tranger,
    const char *topic_name,
    const char *key,
    json_t *match_cond,         // from_rowid, to_rowid, from_t, to_t, backward, …
    tranger2_load_record_callback_t load_record_callback,
    const char *iterator_id,
    const char *creator,
    json_t *data,               // user data for callback
    json_t *extra
);
int     tranger2_close_iterator(json_t *tranger, json_t *iterator);
json_t *tranger2_get_iterator_by_id(json_t *tranger, const char *topic_name,
    const char *iterator_id, const char *creator);
size_t  tranger2_iterator_size(json_t *iterator);
json_t *tranger2_iterator_get_page(json_t *tranger, json_t *iterator,
    json_int_t from_rowid, size_t limit, BOOL backward);
```

**Callback signature:**
```c
typedef int (*tranger2_load_record_callback_t)(
    json_t *tranger, json_t *topic, const char *key,
    json_t *list, json_int_t rowid,
    md2_record_ex_t *md_record_ex,
    json_t *jn_record  // caller must decref
);
// Return: 1 = add to list, 0 = skip, -1 = stop
```

### Reading: Real-Time Streams

```c
// In-memory (latest records, fast)
json_t *tranger2_open_rt_mem(json_t *tranger, const char *topic_name, const char *key,
    json_t *match_cond, tranger2_load_record_callback_t cb,
    const char *list_id, const char *creator, json_t *extra);
int     tranger2_close_rt_mem(json_t *tranger, json_t *mem);
json_t *tranger2_get_rt_mem_by_id(json_t *tranger, const char *topic_name,
    const char *rt_id, const char *creator);

// Disk-backed (persistent, slower)
json_t *tranger2_open_rt_disk(json_t *tranger, const char *topic_name, const char *key,
    json_t *match_cond, tranger2_load_record_callback_t cb,
    const char *rt_id, const char *creator, json_t *extra);
int     tranger2_close_rt_disk(json_t *tranger, json_t *disk);
json_t *tranger2_get_rt_disk_by_id(json_t *tranger, const char *topic_name,
    const char *rt_id, const char *creator);

// Unified list (combines iterator + optional rt_mem or rt_disk)
json_t *tranger2_open_list(json_t *tranger, const char *topic_name,
    json_t *match_cond, json_t *extra, const char *rt_id,
    BOOL rt_by_disk, const char *creator);
int     tranger2_close_list(json_t *tranger, json_t *list);
int     tranger2_close_all_lists(json_t *tranger, const char *topic_name,
    const char *rt_id, const char *creator);
```

### Record Flags

```c
typedef enum {
    sf_string_key,          // primary key is a string
    sf_rowid_key,           // primary key is rowid
    sf_int_key,             // primary key is integer
    sf_zip_record,          // records are compressed
    sf_cipher_record,       // records are encrypted
    sf_t_ms,                // timestamp in milliseconds
    sf_tm_ms,               // message time in milliseconds
    sf_deleted_record,      // record is deleted
} system_flag2_t;
```

### TreeDB (tr_treedb)

Schema-driven graph database layered on timeranger2. Provides node/link operations with foreign keys and hooks:

```c
// Database lifecycle
json_t *treedb_open_db(json_t *tranger, const char *treedb_name,
    json_t *jn_schema, const char *options);
int     treedb_close_db(json_t *tranger, const char *treedb_name);
int     treedb_set_callback(json_t *tranger, const char *treedb_name,
    treedb_callback_t cb, void *user_data);

// Topic management
json_t *treedb_list_treedb(json_t *tranger, json_t *kw);
json_t *treedb_topics(json_t *tranger, const char *treedb_name, json_t *jn_options);
size_t  treedb_topic_size(json_t *tranger, const char *treedb_name, const char *topic_name);
BOOL    treedb_is_treedbs_topic(json_t *tranger, const char *treedb_name, const char *topic_name);

// Node CRUD
json_t *treedb_create_node(json_t *tranger, const char *treedb_name,
    const char *topic_name, json_t *kw);
json_t *treedb_update_node(json_t *tranger, json_t *node, json_t *kw, BOOL save);
int     treedb_save_node(json_t *tranger, json_t *node);
int     treedb_delete_node(json_t *tranger, json_t *node, json_t *jn_options);
json_t *treedb_get_node(json_t *tranger, const char *treedb_name,
    const char *topic_name, const char *id);
json_t *treedb_list_nodes(json_t *tranger, const char *treedb_name,
    const char *topic_name, json_t *jn_filter,
    BOOL (*match_fn)(json_t *topic_desc, json_t *node, json_t *jn_filter));

// Instances (multi-version records via pkey2)
json_t *treedb_get_instance(json_t *tranger, const char *treedb_name,
    const char *topic_name, const char *pkey2_name, const char *id, const char *key2);
json_t *treedb_list_instances(json_t *tranger, const char *treedb_name,
    const char *topic_name, const char *pkey2_name, json_t *jn_filter,
    BOOL (*match_fn)(json_t *topic_desc, json_t *node, json_t *jn_filter));
int     treedb_delete_instance(json_t *tranger, json_t *node,
    const char *pkey2_name, json_t *jn_options);

// Link operations
int     treedb_link_nodes(json_t *tranger, const char *hook,
    json_t *parent_node, json_t *child_node);
int     treedb_unlink_nodes(json_t *tranger, const char *hook,
    json_t *parent_node, json_t *child_node);
int     treedb_autolink(json_t *tranger, json_t *node, json_t *kw, BOOL save);
int     treedb_clean_node(json_t *tranger, json_t *node, BOOL save);

// Navigation
json_t *treedb_parent_refs(json_t *tranger, const char *fkey, json_t *node, json_t *jn_options);
json_t *treedb_list_parents(json_t *tranger, const char *fkey, json_t *node, json_t *jn_options);
json_t *treedb_node_children(json_t *tranger, const char *hook,
    json_t *node, json_t *jn_filter, json_t *jn_options);
json_t *treedb_node_jtree(json_t *tranger, const char *hook, const char *rename_hook,
    json_t *node, json_t *jn_filter, json_t *jn_options);

// Topic links & hooks introspection
json_t *treedb_get_topic_links(json_t *tranger, const char *treedb_name, const char *topic_name);
json_t *treedb_get_topic_hooks(json_t *tranger, const char *treedb_name, const char *topic_name);

// Snapshots
int     treedb_shoot_snap(json_t *tranger, const char *treedb_name,
    const char *snap_name, const char *description);
int     treedb_activate_snap(json_t *tranger, const char *treedb_name, const char *snap_name);
json_t *treedb_list_snaps(json_t *tranger, const char *treedb_name, json_t *filter);
```

### Queue (tr_queue)

**Header:** `timeranger2/src/tr_queue.h`

Persistent message queue built on timeranger2:

```c
tr_queue_t *trq_open(json_t *tranger, const char *topic_name,
    const char *tkey, system_flag2_t system_flag, size_t backup_queue_size);
void trq_close(tr_queue_t *trq);

// Load messages
int trq_load(tr_queue_t *trq);
int trq_load_all(tr_queue_t *trq, int64_t from_rowid, int64_t to_rowid);
int trq_load_all_by_time(tr_queue_t *trq, int64_t from_t, int64_t to_t);

// Message operations
q_msg_t *trq_append2(tr_queue_t *trq, json_int_t t, json_t *kw, uint16_t user_flag);
q_msg_t *trq_get_by_rowid(tr_queue_t *trq, uint64_t rowid);
void     trq_unload_msg(q_msg_t *msg, int32_t result);
int      trq_set_hard_flag(q_msg_t *msg, uint16_t hard_mark, BOOL set);
uint64_t trq_set_soft_mark(q_msg_t *msg, uint64_t soft_mark, BOOL set);

// Message content
md2_record_ex_t *trq_msg_md(q_msg_t *msg);
json_t          *trq_msg_json(q_msg_t *msg);

// Metadata & backup
int     trq_set_metadata(json_t *kw, const char *key, json_t *jn_value);
json_t *trq_get_metadata(json_t *kw);
int     trq_check_backup(tr_queue_t *trq);
json_t *trq_answer(json_t *jn_message, int result);
```

### Message Instances (tr_msg)

**Header:** `timeranger2/src/tr_msg.h`

Key-value message store with active/inactive instance tracking:

```c
int     trmsg_open_topics(json_t *tranger, const topic_desc_t *descs);
int     trmsg_close_topics(json_t *tranger, const topic_desc_t *descs);
int     trmsg_add_instance(json_t *tranger, const char *topic_name,
    json_t *jn_msg, md2_record_ex_t *md_record);

json_t *trmsg_open_list(json_t *tranger, const char *topic_name,
    json_t *match_cond, json_t *extra, const char *rt_id,
    BOOL rt_by_disk, const char *creator);
int     trmsg_close_list(json_t *tranger, json_t *list);

// Accessors (non-owned returns)
json_t *trmsg_get_messages(json_t *list);
json_t *trmsg_get_message(json_t *list, const char *key);
json_t *trmsg_get_active_message(json_t *list, const char *key);
json_t *trmsg_get_instances(json_t *list, const char *key);

// Data extraction (owned returns)
json_t *trmsg_data_tree(json_t *list, json_t *jn_filter);
json_t *trmsg_active_records(json_t *list, json_t *jn_filter);
json_t *trmsg_record_instances(json_t *list, const char *key, json_t *jn_filter);

// Iteration
int trmsg_foreach_active_messages(json_t *list,
    int (*callback)(json_t *list, const char *key, json_t *record,
        void *user_data1, void *user_data2),
    void *user_data1, void *user_data2, json_t *jn_filter);
```

### File System Watcher (fs_watcher)

**Header:** `timeranger2/src/fs_watcher.h`

```c
fs_event_t *fs_create_watcher_event(yev_loop_h yev_loop, const char *path,
    fs_flag_t fs_flag, fs_callback_t callback, hgobj gobj,
    void *user_data, void *user_data2);
int fs_start_watcher_event(fs_event_t *fs_event);
int fs_stop_watcher_event(fs_event_t *fs_event);
```

---

## ytls — TLS Abstraction

**Header:** `ytls/src/ytls.h`

Pluggable TLS layer supporting OpenSSL and mbed-TLS backends. Used by `C_TCP` for encrypted connections.

```c
// Initialize TLS context
hytls ytls_init(hgobj gobj, json_t *jn_config, BOOL server);
void  ytls_cleanup(hytls ytls);
const char *ytls_version(hytls ytls);

// Per-connection secure filter
hsskt ytls_new_secure_filter(hytls ytls,
    int (*on_handshake_done_cb)(void *user_data, int error),
    int (*on_clear_data_cb)(void *user_data, gbuffer_t *gbuf),
    int (*on_encrypted_data_cb)(void *user_data, gbuffer_t *gbuf),
    void *user_data);
void ytls_free_secure_filter(hytls ytls, hsskt sskt);

// TLS operations
int ytls_do_handshake(hytls ytls, hsskt sskt);  // 1=done, 0=in-progress, -1=fail
int ytls_encrypt_data(hytls ytls, hsskt sskt, gbuffer_t *gbuf);
int ytls_decrypt_data(hytls ytls, hsskt sskt, gbuffer_t *gbuf);
int ytls_flush(hytls ytls, hsskt sskt);
void ytls_shutdown(hytls ytls, hsskt sskt);

// Diagnostics
const char *ytls_get_last_error(hytls ytls, hsskt sskt);
void ytls_set_trace(hytls ytls, hsskt sskt, BOOL set);
```

**Key config fields:** `library` ("openssl"/"mbedtls"), `ssl_certificate`, `ssl_certificate_key`, `ssl_trusted_certificate`, `ssl_ciphers`, `ssl_verify_depth`, `rx_buffer_size`, `trace`

---

## libjwt — JSON Web Tokens

**Header:** `libjwt/src/jwt.h`

JWT creation, parsing, and verification with support for HS256/384/512, RS256/384/512, ES256/384/512, PS256/384/512, EdDSA.

```c
// Algorithm utilities
jwt_alg_t   jwt_get_alg(const jwt_t *jwt);
const char *jwt_alg_str(jwt_alg_t alg);
jwt_alg_t   jwt_str_alg(const char *alg);

// Builder (create and sign tokens)
jwt_builder_t *jwt_builder_new(void);
void jwt_builder_free(jwt_builder_t *builder);
int  jwt_builder_setkey(jwt_builder_t *builder, jwt_alg_t alg, const jwk_item_t *key);
int  jwt_builder_enable_iat(jwt_builder_t *builder, int enable);
int  jwt_builder_time_offset(jwt_builder_t *builder, jwt_claims_t claim, time_t secs);
char *jwt_builder_generate(jwt_builder_t *builder);
int   jwt_builder_error(const jwt_builder_t *builder);
const char *jwt_builder_error_msg(const jwt_builder_t *builder);

// Builder claims
jwt_value_error_t jwt_builder_claim_set(jwt_builder_t *builder, jwt_value_t *value);
jwt_value_error_t jwt_builder_claim_get(jwt_builder_t *builder, jwt_value_t *value);
jwt_value_error_t jwt_builder_claim_del(jwt_builder_t *builder, const char *claim);
jwt_value_error_t jwt_builder_header_set(jwt_builder_t *builder, jwt_value_t *value);
jwt_value_error_t jwt_builder_header_get(jwt_builder_t *builder, jwt_value_t *value);

// Checker (verify and decode tokens)
jwt_checker_t *jwt_checker_new(void);
void jwt_checker_free(jwt_checker_t *checker);
int  jwt_checker_setkey(jwt_checker_t *checker, jwt_alg_t alg, const jwk_item_t *key);
int  jwt_checker_verify(jwt_checker_t *checker, const char *token);
json_t *jwt_checker_verify2(jwt_checker_t *checker, const char *token);
int  jwt_checker_time_leeway(jwt_checker_t *checker, jwt_claims_t claim, time_t secs);
int  jwt_checker_error(const jwt_checker_t *checker);
const char *jwt_checker_error_msg(const jwt_checker_t *checker);

// Token claim access (on decoded jwt_t)
jwt_value_error_t jwt_claim_get(jwt_t *jwt, jwt_value_t *value);
jwt_value_error_t jwt_header_get(jwt_t *jwt, jwt_value_t *value);

// JWK sets (local, file, or remote)
jwk_set_t *jwks_create(const char *jwk_json_str);
jwk_set_t *jwks_load(jwk_set_t *jwk_set, const char *jwk_json_str);
jwk_set_t *jwks_create_fromfile(const char *file_name);
jwk_set_t *jwks_create_fromurl(const char *url, int verify);
void jwks_free(jwk_set_t *jwk_set);
jwk_item_t *jwks_find_bykid(jwk_set_t *jwk_set, const char *kid);
const jwk_item_t *jwks_item_get(const jwk_set_t *jwk_set, size_t index);
size_t jwks_item_count(const jwk_set_t *jwk_set);
int jwks_error(const jwk_set_t *jwk_set);
const char *jwks_error_msg(const jwk_set_t *jwk_set);

// JWK item introspection
int jwks_item_is_private(const jwk_item_t *item);
const char *jwks_item_kid(const jwk_item_t *item);
jwt_alg_t   jwks_item_alg(const jwk_item_t *item);
jwk_key_type_t jwks_item_kty(const jwk_item_t *item);
int jwks_item_key_bits(const jwk_item_t *item);

// Crypto provider
const char *jwt_get_crypto_ops(void);
int jwt_set_crypto_ops(const char *opname);
int jwt_crypto_ops_supports_jwk(void);
```

---

## root-linux — Runtime GClasses

**Location:** `root-linux/src/`

33 GClasses providing network, protocol, storage, and system functionality. All registered via `yunetas_register_c_core()`.

### Entry Point

**Header:** `entry_point.h`

```c
// Setup memory and callbacks (call before yuneta_entry_point)
int yuneta_setup(
    const persistent_attrs_t *persistent_attrs,
    json_function_fn command_parser,
    json_function_fn stats_parser,
    authorization_checker_fn authz_checker,
    authentication_parser_fn authentication_parser,
    size_t mem_max_block,
    size_t mem_max_system_memory,
    BOOL use_own_system_memory,
    size_t mem_min_block,
    size_t mem_superblock
);

// Main entry point (parses args, loads config, starts event loop)
int yuneta_entry_point(
    int argc, char *argv[],
    const char *APP_NAME,
    const char *APP_VERSION,
    const char *APP_SUPPORT,
    const char *APP_DOC,
    const char *APP_DATETIME,
    const char *fixed_config,
    const char *variable_config,
    int (*register_yuno_and_more)(void),
    void (*cleaning_fn)(void)
);

void    set_auto_kill_time(int seconds);
json_t *yuneta_json_config(void);
```

### Registration

```c
#include "yunetas_register.h"

int yunetas_register_c_core(void);  // register all built-in GClasses
```

### Yuno Extras

**Header:** `c_yuno.h`

```c
void *yuno_event_loop(void);        // get the yev_loop_h
void  yuno_event_detroy(void);
void  set_yuno_must_die(void);

// IP allow/deny lists
BOOL is_ip_allowed(const char *peername);
int  add_allowed_ip(const char *ip, BOOL allowed);
int  remove_allowed_ip(const char *ip);
BOOL is_ip_denied(const char *peername);
int  add_denied_ip(const char *ip, BOOL denied);
int  remove_denied_ip(const char *ip);
```

### GClass Reference

**Network & Transport:**

| GClass | Purpose | Key attributes |
|--------|---------|----------------|
| `C_TCP` | TCP client with TLS | `url`, `use_ssl`, `connected` |
| `C_TCP_S` | TCP server with TLS | `url`, `use_ssl` |
| `C_UDP` | UDP client | `url`, `lHost`, `lPort` |
| `C_UDP_S` | UDP server | `url` |
| `C_UART` | Serial port | `tty_name`, `baudrate` |
| `C_PTY` | Pseudo-terminal | `command` |

**Protocol Handlers:**

| GClass | Purpose |
|--------|---------|
| `C_WEBSOCKET` | WebSocket (client + server) |
| `C_PROT_HTTP_CL` | HTTP client |
| `C_PROT_HTTP_SR` | HTTP server |
| `C_PROT_MQTT` | MQTT v3.1.1 client/broker protocol |
| `C_PROT_TCP4H` | TCP 4-byte-header framing |
| `C_PROT_RAW` | Raw passthrough |

**Message Routing:**

| GClass | Purpose |
|--------|---------|
| `C_IOGATE` | Central message routing gateway |
| `C_MQIOGATE` | Multi-queue IOGATE |
| `C_CHANNEL` | Layered protocol channel |
| `C_IEVENT_CLI` | Inter-event client (connects to remote yuno) |
| `C_IEVENT_SRV` | Inter-event server (accepts remote yunos) |

**Persistence & Storage:**

| GClass | Purpose |
|--------|---------|
| `C_TREEDB` | TreeDB interface (schema-driven graph DB) |
| `C_TRANGER` | Timeranger2 interface (time-series log) |
| `C_NODE` | TreeDB node CRUD operations |
| `C_RESOURCE2` | Generic resource management |

**System & Utilities:**

| GClass | Purpose |
|--------|---------|
| `C_YUNO` | Yuno lifecycle (start/stop/update/stats) |
| `C_TIMER` | High-precision timers |
| `C_TIMER0` | Low-level timer primitives |
| `C_AUTHZ` | Authorization engine |
| `C_FS` | File system operations (watch, read, write) |
| `C_COUNTER` | Stats counter |
| `C_OTA` | Over-the-air binary update |
| `C_TASK` | Task sequencer |
| `C_TASK_AUTHENTICATE` | Authentication task |

### Common Events

```c
EV_ON_OPEN          // connection established
EV_ON_CLOSE         // connection closed
EV_ON_MESSAGE       // data received
EV_ON_ID            // peer identified
EV_ON_ID_NAK        // peer identification rejected
EV_TIMEOUT          // one-shot timer fired
EV_TIMEOUT_PERIODIC // periodic timer fired
EV_DROP             // drop connection
EV_SEND_MESSAGE     // send data
```

---

## root-esp32 — ESP32 Port

**Location:** `root-esp32/`

ESP32-specific components maintaining GObject compatibility for embedded targets. Provides hardware integration for ESP32 SoC:

- `esp_gobj/` — GObject wrapper for ESP-IDF
- `esp_yuneta/` — Yuneta bootstrap for ESP32
- `esp_jansson/` — jansson JSON for ESP32
- `esp_c_prot/` — Protocol GClasses for ESP32
- `ads111x/` — ADS111x ADC driver
- `i2cdev/` — I2C device abstraction

Builds as a static library `libyunetas-esp32.a` via CMake + ESP-IDF.

---

## linux-ext-libs — External Dependencies

**Location:** `kernel/c/linux-ext-libs/`

Vendored external libraries. Build before the kernel modules:

```bash
./set_compiler.sh           # sync compiler choice from .config
./extrae.sh                 # clone libraries
./configure-libs.sh         # configure, build and install to outputs_ext/

# For musl/static builds:
./extrae-musl.sh
./configure-libs-musl.sh
```

Includes: **jansson** (JSON), **OpenSSL** (TLS/crypto), **liburing** (io_uring), and others.

---

## Writing a Custom GClass

```c
#include <yuneta.h>

// ─── Private data ──────────────────────────────────────────

typedef struct {
    // mirror frequently-used attrs here
    json_int_t timeout_ms;
    BOOL       connected;
} priv_t;

// ─── Attribute schema ──────────────────────────────────────

PRIVATE sdata_desc_t attrs_table[] = {
    SDATA(DTP_INTEGER, "timeout_ms", SDF_RD, 5000, "Timeout in milliseconds"),
    SDATA(DTP_BOOLEAN, "connected",  SDF_RD, FALSE, "Connection state"),
    SDATA(DTP_STRING,  "url",        SDF_RD, "",    "Remote URL"),
    SDATA_END()
};

// ─── FSM ────────────────────────────────────────────────────

PRIVATE EV_ACTION ST_IDLE[] = {
    {EV_CONNECT,    ac_connect,    ST_CONNECTED},
    {0,0,0}
};
PRIVATE EV_ACTION ST_CONNECTED[] = {
    {EV_DISCONNECT, ac_disconnect, ST_IDLE},
    {EV_ON_MESSAGE, ac_message,    0},     // 0 = stay in current state
    {0,0,0}
};

PRIVATE const char *st_names[] = {
    "ST_IDLE",
    "ST_CONNECTED",
    NULL
};
PRIVATE ev_action_t *states[] = {
    ST_IDLE,
    ST_CONNECTED,
    NULL
};
PRIVATE event_type_t event_types[] = {
    {EV_CONNECT,    EVF_OUTPUT_EVENT},   // output: published to subscribers
    {EV_DISCONNECT, 0},
    {EV_ON_MESSAGE, EVF_OUTPUT_EVENT},
    {0, 0}
};

// ─── GClass descriptor ─────────────────────────────────────

PRIVATE GOBJ_DEFINE_GCLASS(C_MY_CLASS);

// ─── Lifecycle methods ──────────────────────────────────────

PRIVATE int mt_create(hgobj gobj) {
    priv_t *priv = gobj_priv_data(gobj);
    // mirror attrs to priv
    priv->timeout_ms = gobj_read_integer_attr(gobj, "timeout_ms");
    return 0;
}

PRIVATE int mt_writing(hgobj gobj, const char *attr) {
    priv_t *priv = gobj_priv_data(gobj);
    // keep priv in sync when attrs are written
    IF_EQ_SET_PRIV(timeout_ms, gobj_read_integer_attr(gobj, "timeout_ms"))
    ELIF_EQ_SET_PRIV(connected, gobj_read_bool_attr(gobj, "connected"))
    return 0;
}

PRIVATE int mt_start(hgobj gobj) {
    return 0;
}

PRIVATE int mt_stop(hgobj gobj) {
    return 0;
}

// ─── Action functions ───────────────────────────────────────

PRIVATE json_t *ac_connect(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src) {
    gobj_write_bool_attr(gobj, "connected", TRUE);
    gobj_publish_event(gobj, EV_CONNECT, json_object());
    KW_DECREF(kw);
    return 0;
}

PRIVATE json_t *ac_disconnect(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src) {
    gobj_write_bool_attr(gobj, "connected", FALSE);
    gobj_publish_event(gobj, EV_DISCONNECT, json_object());
    KW_DECREF(kw);
    return 0;
}

PRIVATE json_t *ac_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src) {
    // process kw data...
    gobj_publish_event(gobj, EV_ON_MESSAGE, kw);  // pass kw ownership to subscribers
    return 0;
}

// ─── Registration ───────────────────────────────────────────

PRIVATE gobj_methods_t mt = {
    .mt_create  = mt_create,
    .mt_writing = mt_writing,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
};

PUBLIC int register_c_my_class(void) {
    return gclass_register(
        &C_MY_CLASS,
        "C_MY_CLASS",
        event_types,
        states,
        st_names,
        &mt,
        NULL,               // lmt (low-level methods)
        attrs_table,
        sizeof(priv_t),
        NULL,               // authz_table
        NULL,               // command_table
        NULL,               // s_user_trace_level
        0                   // gclass_flag
    );
}
```

---

## Source Layout

```
kernel/c/
├── README.md                        ← this file
├── linux-ext-libs/                  ← external library sources and build scripts
│   ├── extrae.sh / extrae-musl.sh
│   ├── configure-libs.sh / configure-libs-musl.sh
│   ├── re-install-libs.sh / re-install-libs-musl.sh
│   └── sources/
│
├── gobj-c/src/                      ← GObject framework (~12K-line core)
│   ├── gobj.h                       ← ALL public API (macros, types, FSM API)
│   ├── gobj.c                       ← GObject runtime
│   ├── helpers.h / helpers.c        ← kw ops, string utils, JSON helpers
│   ├── kwid.h / kwid.c              ← key-value ID utilities
│   ├── gbuffer.h / gbuffer.c        ← growable buffer
│   ├── gbmem.h / gbmem.c            ← memory tracking
│   ├── glogger.h / glogger.c        ← structured logging
│   ├── rotatory.h / rotatory.c      ← log rotation
│   ├── dl_list.h / dl_list.c        ← double-linked list
│   ├── 00_http_parser.h / .c        ← HTTP parser
│   └── testing.h / testing.c        ← test utilities
│
├── libjwt/src/                      ← JWT (~180K)
│   ├── jwt.h                        ← public JWT API
│   ├── jwt.c, jwt-builder.c, jwt-verify.c, jwks.c
│   └── openssl/ gnutls/ mbedtls/    ← crypto backends
│
├── ytls/src/                        ← TLS abstraction
│   ├── ytls.h                       ← pluggable TLS API
│   ├── ytls.c
│   └── tls/                         ← openssl, mbedtls backends
│
├── yev_loop/src/                    ← io_uring event loop
│   ├── yev_loop.h
│   └── yev_loop.c
│
├── timeranger2/src/                 ← time-series DB + TreeDB
│   ├── timeranger2.h
│   ├── timeranger2.c
│   ├── tr_treedb.h / tr_treedb.c   ← graph DB on top of timeranger2
│   ├── tr_queue.h / tr_queue.c     ← queue structure
│   ├── tr_msg.h / tr_msg.c         ← message handling
│   └── fs_watcher.h / fs_watcher.c ← inotify file watcher
│
├── root-linux/src/                  ← 33 runtime GClasses
│   ├── c_tcp.c / c_tcp.h
│   ├── c_tcp_s.c / c_tcp_s.h
│   ├── c_timer.c / c_timer.h
│   ├── c_iogate.c / c_iogate.h
│   ├── c_channel.c / c_channel.h
│   ├── c_ievent_cli.c / c_ievent_cli.h
│   ├── c_ievent_srv.c / c_ievent_srv.h
│   ├── c_treedb.c / c_treedb.h
│   ├── c_tranger.c / c_tranger.h
│   ├── c_authz.c / c_authz.h
│   ├── c_yuno.c / c_yuno.h
│   ├── c_websocket.c / c_websocket.h
│   ├── c_prot_http_cl.c, c_prot_http_sr.c
│   ├── msg_ievent.h / msg_ievent.c  ← inter-event wire format
│   ├── entry_point.h / entry_point.c
│   └── yunetas_register.h / .c
│
└── root-esp32/                      ← ESP32 embedded components
    ├── common_components/
    └── components/
```
