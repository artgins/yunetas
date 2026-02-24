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
  - [Attribute Schema (SData)](#attribute-schema-sdata)
  - [GObject Lifecycle API](#gobject-lifecycle-api)
  - [State Machine API](#state-machine-api)
  - [Attribute Access API](#attribute-access-api)
  - [Event System API](#event-system-api)
  - [Hierarchy & Navigation](#hierarchy--navigation)
  - [Authorization](#authorization)
  - [Tracing & Debugging](#tracing--debugging)
  - [Helpers & Utilities](#helpers--utilities)
  - [Logging](#logging)
- [yev_loop — Async Event Loop](#yev_loop--async-event-loop)
- [timeranger2 — Time-Series Database](#timeranger2--time-series-database)
- [ytls — TLS Abstraction](#ytls--tls-abstraction)
- [libjwt — JSON Web Tokens](#libjwt--json-web-tokens)
- [root-linux — Runtime GClasses](#root-linux--runtime-gclasses)
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

# 2. Build external libraries (once, or after compiler change)
./set_compiler.sh
cd kernel/c/linux-ext-libs
./extrae.sh && ./install-libs.sh

# 3. Configure
menuconfig      # edit .config: compiler, build type, optional modules

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

### Attribute Schema (SData)

```c
PRIVATE sdata_desc_t tattr_desc[] = {
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
    json_t *jn_global_settings,
    int (*load_persistent_attrs)(hgobj gobj, json_t *keys),
    int (*save_persistent_attrs)(hgobj gobj, json_t *keys),
    int (*remove_persistent_attrs)(hgobj gobj, json_t *keys),
    json_t *(*list_persistent_attrs)(hgobj gobj, json_t *keys),
    json_function_t global_command_parser,
    json_function_t global_stats_parser
);

// Creation
hgobj gobj_create(                                              // [JS]
    const char *name,
    gclass_t gclass,      // e.g. C_MY_CLASS
    json_t *kw,           // initial attribute values
    hgobj parent
);
hgobj gobj_create_yuno(const char *name, gclass_t gclass, json_t *kw);             // [JS]
hgobj gobj_create_service(const char *name, gclass_t gclass, json_t *kw, hgobj yuno);  // [JS]
hgobj gobj_create_default_service(const char *name, gclass_t gclass, json_t *kw, hgobj yuno);  // [JS]
hgobj gobj_create_volatil(const char *name, gclass_t gclass, json_t *kw, hgobj parent);  // [JS]
hgobj gobj_create_pure_child(const char *name, gclass_t gclass, json_t *kw, hgobj parent);  // [JS]
hgobj gobj_create_tree(hgobj parent, json_t *jn_tree_config, ...);

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

// Status queries
BOOL gobj_is_running(hgobj gobj);                               // [JS]
BOOL gobj_is_playing(hgobj gobj);                               // [JS]
BOOL gobj_is_destroying(hgobj gobj);                            // [JS]
BOOL gobj_is_volatil(hgobj gobj);                               // [JS]
BOOL gobj_is_pure_child(hgobj gobj);                            // [JS]
BOOL gobj_is_disabled(hgobj gobj);
```

### State Machine API

```c
int         gobj_change_state(hgobj gobj, gobj_state_t new_state);  // [JS]
const char *gobj_current_state(hgobj gobj);                         // [JS]
BOOL        gobj_in_this_state(hgobj gobj, gobj_state_t state);
BOOL        gobj_has_event(hgobj gobj, gobj_event_t event);         // [JS]
BOOL        gobj_has_output_event(hgobj gobj, gobj_event_t event);  // [JS]
```

### Attribute Access API

```c
// Generic
json_t *gobj_read_attr(hgobj gobj, const char *name, hgobj src);                   // [JS]
int     gobj_write_attr(hgobj gobj, const char *name, json_t *value, hgobj src);    // [JS]
json_t *gobj_read_attrs(hgobj gobj, sdata_flag_t include_flag, hgobj src);          // [JS]
int     gobj_write_attrs(hgobj gobj, json_t *kw, sdata_flag_t flag, hgobj src);     // [JS]
BOOL    gobj_has_attr(hgobj gobj, const char *name);                                // [JS]

// Typed reads
const char *gobj_read_str_attr(hgobj gobj, const char *name);                       // [JS]
json_int_t  gobj_read_integer_attr(hgobj gobj, const char *name);                   // [JS]
double      gobj_read_real_attr(hgobj gobj, const char *name);
BOOL        gobj_read_bool_attr(hgobj gobj, const char *name);                      // [JS]
json_t     *gobj_read_json_attr(hgobj gobj, const char *name);
void       *gobj_read_pointer_attr(hgobj gobj, const char *name);                   // [JS]

// Typed writes
int gobj_write_str_attr(hgobj gobj, const char *name, const char *value);            // [JS]
int gobj_write_integer_attr(hgobj gobj, const char *name, json_int_t value);         // [JS]
int gobj_write_real_attr(hgobj gobj, const char *name, double value);
int gobj_write_bool_attr(hgobj gobj, const char *name, BOOL value);                  // [JS]
int gobj_write_json_attr(hgobj gobj, const char *name, json_t *value);
int gobj_write_pointer_attr(hgobj gobj, const char *name, void *value);

// Stats
int gobj_reset_volatil_attrs(hgobj gobj);
int gobj_reset_rstats_attrs(hgobj gobj);
```

### Event System API

```c
// Sending events
json_t *gobj_send_event(hgobj dst, gobj_event_t event, json_t *kw, hgobj src);     // [JS]
json_t *gobj_publish_event(hgobj publisher, gobj_event_t event, json_t *kw);        // [JS]

// Subscriptions
hsdata gobj_subscribe_event(                                                         // [JS]
    hgobj publisher,
    gobj_event_t event,
    json_t *kw,           // subscription options (filter, config)
    hgobj subscriber
);
int gobj_unsubscribe_event(hgobj publisher, gobj_event_t event, json_t *kw, hgobj subscriber);  // [JS]
int gobj_unsubscribe_list(dl_list_t *dl_subs, BOOL force);                          // [JS]

json_t *gobj_find_subscriptions(hgobj gobj, gobj_event_t event, json_t *kw, hgobj subscriber);  // [JS]
json_t *gobj_list_subscriptions(hgobj gobj);                                        // [JS]
json_t *gobj_find_subscribings(hgobj gobj, gobj_event_t event, json_t *kw, hgobj publisher);  // [JS]

// Commands & Stats
json_t *gobj_command(hgobj gobj, const char *command, json_t *kw, hgobj src);       // [JS]
json_t *gobj_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src);           // [JS]
```

### Hierarchy & Navigation

```c
hgobj       gobj_parent(hgobj gobj);                            // [JS]
hgobj       gobj_yuno(void);                                    // [JS]
hgobj       gobj_default_service(void);                         // [JS]

const char *gobj_name(hgobj gobj);                              // [JS]
const char *gobj_short_name(hgobj gobj);                        // [JS]
char       *gobj_full_name(hgobj gobj);      // caller must free  [JS]
const char *gobj_gclass_name(hgobj gobj);                       // [JS]
const char *gobj_yuno_name(void);                               // [JS]
const char *gobj_yuno_role(void);                               // [JS]
const char *gobj_yuno_id(void);                                 // [JS]

hgobj gobj_find_child(hgobj gobj, json_t *kw_filter);           // [JS]
hgobj gobj_find_service(const char *name, BOOL verbose);         // [JS]
hgobj gobj_find_gobj(const char *path);                          // [JS]
hgobj gobj_search_path(hgobj gobj, const char *path);            // [JS]

int gobj_walk_gobj_children(hgobj gobj, walk_type_t walk_type,              // [JS]
    cb_walking_t cb, void *user_data);
int gobj_walk_gobj_children_tree(hgobj gobj, walk_type_t walk_type,         // [JS]
    cb_walking_t cb, void *user_data);
```

### Authorization

```c
// Register authz checker (called at gobj_start_up)
typedef BOOL (*authz_checker_fn)(hgobj gobj, const char *authz,
    json_t *kw, hgobj src);

BOOL gobj_user_has_authz(hgobj gobj, const char *authz,
    json_t *kw, hgobj src);
json_t *gobj_authenticate(hgobj gobj, json_t *kw, hgobj src);
json_t *gobj_authz(hgobj gobj, const char *authz, hgobj src);
json_t *gobj_authzs(hgobj gobj, hgobj src);
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

// Set trace
int gobj_set_global_trace(const char *level, BOOL set);
int gobj_set_gclass_trace(gclass_t gclass, const char *level, BOOL set);
int gobj_set_gobj_trace(hgobj gobj, const char *level, BOOL set);
int gobj_set_gclass_no_trace(gclass_t gclass, const char *level, BOOL set);
int gobj_set_global_no_trace(const char *level, BOOL set);

// Query trace
uint32_t gobj_get_gobj_trace_level(hgobj gobj);
uint32_t gobj_get_gclass_trace_level(gclass_t gclass);
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
BOOL        kw_get_bool(hgobj gobj, json_t *kw, const char *key, int def, kw_flag_t flag);  // [JS]
json_int_t  kw_get_int(hgobj gobj, json_t *kw, const char *key, json_int_t def, kw_flag_t flag);  // [JS]
double      kw_get_real(hgobj gobj, json_t *kw, const char *key, double def, kw_flag_t flag);  // [JS]
const char *kw_get_str(hgobj gobj, json_t *kw, const char *key, const char *def, kw_flag_t flag);  // [JS]
json_t     *kw_get_dict(hgobj gobj, json_t *kw, const char *key, json_t *def, kw_flag_t flag);  // [JS]
json_t     *kw_get_list(hgobj gobj, json_t *kw, const char *key, json_t *def, kw_flag_t flag);  // [JS]

// kw manipulation
json_t *kw_duplicate(json_t *kw);
BOOL    kw_has_key(json_t *kw, const char *key);                                    // [JS]
int     kw_delete(json_t *kw, const char *key);                                     // [JS]
json_t *kw_find_path(hgobj gobj, json_t *kw, const char *path, BOOL verbose);       // [JS]

BOOL    kw_match_simple(json_t *kw, json_t *kw_filter);                             // [JS]
json_t *kw_select(json_t *list, json_t *kw_filter);                                 // [JS]
json_t *kw_collect(json_t *kw, json_t *keys);                                       // [JS]
json_t *kw_clone_by_keys(json_t *kw, json_t *keys, BOOL notkey);                    // [JS]

// Set values (dot-separated path)
int kw_set_dict_value(hgobj gobj, json_t *kw, const char *path, json_t *value);     // [JS]
int kw_set_subdict_value(hgobj gobj, json_t *kw,                                    // [JS]
    const char *subdict, const char *key, json_t *value);
```

**String utilities:**
```c
BOOL empty_string(const char *s);                                                    // [JS]
BOOL str_in_list(const char **list, const char *str, BOOL ignore_case);              // [JS]
int  str2json(const char *s, json_t **jn);
```

**Inter-event message stack:**
```c
int     msg_iev_push_stack(hgobj gobj, json_t *kw, const char *msg_type);           // [JS]
json_t *msg_iev_get_stack(hgobj gobj, json_t *kw, BOOL verbose);                    // [JS]
int     msg_iev_set_msg_type(hgobj gobj, json_t *kw, const char *msg_type);         // [JS]
const char *msg_iev_get_msg_type(hgobj gobj, json_t *kw);                           // [JS]
int     msg_iev_write_key(hgobj gobj, json_t *kw, const char *key, json_t *value);  // [JS]
json_t *msg_iev_read_key(hgobj gobj, json_t *kw, const char *key);                  // [JS]
```

### Logging

```c
// Core log functions (JS: log_error, log_warning, log_info, log_debug)
void gobj_log_error(hgobj gobj, int opt, ...);    // opt: LOG_OPT_*  [JS] as log_error()
void gobj_log_warning(hgobj gobj, int opt, ...);                     // [JS] as log_warning()
void gobj_log_info(hgobj gobj, int opt, ...);                        // [JS] as log_info()
void gobj_log_debug(hgobj gobj, int opt, ...);                       // [JS] as log_debug()

// Convenience macros (variadic key-value pairs terminated by NULL)
gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
    "function",     __FUNCTION__,
    "msgset",       MSGSET_PARAMETER_ERROR,
    "msg",          "Invalid parameter",
    "key",          key,
    NULL
);

// Trace output
void trace_msg(const char *fmt, ...);                                // [JS]
void trace_json(json_t *jn, const char *fmt, ...);                   // [JS]
```

---

## yev_loop — Async Event Loop

**Header:** `yev_loop/src/yev_loop.h`
**Implementation:** `yev_loop/src/yev_loop.c`

Single-threaded, non-blocking I/O engine based on **Linux io_uring**. All I/O, timers, and network events are driven from a single loop with zero blocking. Scaling is achieved by running one yuno per CPU core with inter-yuno messaging.

### Loop Management

```c
yev_loop_t *yev_loop_create(
    hgobj gobj,
    uint32_t entries,           // io_uring queue depth
    int keep_alive_ms,          // heartbeat interval
    yev_callback_t callback     // called for all events
);
void yev_loop_destroy(yev_loop_t *yev_loop);
int  yev_loop_run(yev_loop_t *yev_loop, int timeout_ms);
int  yev_loop_run_once(yev_loop_t *yev_loop);
void yev_loop_stop(yev_loop_t *yev_loop);
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
// Create events
yev_event_t *yev_create_timer_event(yev_loop_t *loop, yev_callback_t cb, hgobj gobj);
yev_event_t *yev_create_connect_event(yev_loop_t *loop, yev_callback_t cb, hgobj gobj);
yev_event_t *yev_create_accept_event(yev_loop_t *loop, yev_callback_t cb, hgobj gobj, int fd);
yev_event_t *yev_create_read_event(yev_loop_t *loop, yev_callback_t cb, hgobj gobj, int fd, gbuffer_t *gbuf);
yev_event_t *yev_create_write_event(yev_loop_t *loop, yev_callback_t cb, hgobj gobj, int fd, gbuffer_t *gbuf);
yev_event_t *yev_create_recvmsg_event(yev_loop_t *loop, yev_callback_t cb, hgobj gobj, int fd, gbuffer_t *gbuf);
yev_event_t *yev_create_sendmsg_event(yev_loop_t *loop, yev_callback_t cb, hgobj gobj, int fd, gbuffer_t *gbuf);

// Arm / Disarm
int  yev_start_event(yev_event_t *ev);
int  yev_start_timer_event(yev_event_t *ev, json_int_t timeout_ms, BOOL periodic);
int  yev_stop_event(yev_event_t *ev);          // idempotent
void yev_destroy_event(yev_event_t *ev);

// Inspection (inline)
int          yev_get_fd(yev_event_t *ev);
yev_state_t  yev_get_state(yev_event_t *ev);
int          yev_get_result(yev_event_t *ev);  // io_uring result code
gbuffer_t   *yev_get_gbuf(yev_event_t *ev);
hgobj        yev_get_gobj(yev_event_t *ev);
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
json_t *tranger2_startup(
    hgobj gobj,
    json_t *jn_config     // path, database, master, trace_level, …
);
int tranger2_stop(json_t *tranger);
int tranger2_shutdown(json_t *tranger);
```

Key config fields: `path` (required), `database`, `master` (BOOL — enables write), `filename_mask` (strftime format), `xpermission`, `rpermission`

### Topic Management

```c
json_t *tranger2_create_topic(json_t *tranger, const char *topic_name,
    const char *pkey, const char *tkey, json_t *jn_cols, json_t *jn_topic_ext,
    system_flag2_t system_flag);

json_t *tranger2_open_topic(json_t *tranger, const char *topic_name, BOOL verbose);
json_t *tranger2_topic(json_t *tranger, const char *topic_name);  // open if needed
int     tranger2_close_topic(json_t *tranger, const char *topic_name);
int     tranger2_delete_topic(json_t *tranger, const char *topic_name);

json_t *tranger2_list_topics(json_t *tranger, BOOL verbose);
json_t *tranger2_list_topic_names(json_t *tranger);
const char *tranger2_topic_path(json_t *tranger, const char *topic_name);
```

### Writing Records

```c
int tranger2_append_record(
    json_t *tranger,
    json_t *topic,
    uint64_t __t__,         // timestamp (0 = now)
    uint32_t user_flag,
    md2_record_ex_t *md_record_ex,  // out: metadata
    json_t *jn_record               // data to write (owned)
);

int tranger2_delete_record(json_t *tranger, json_t *topic,
    const char *key, json_int_t rowid);
int tranger2_set_user_flag(json_t *tranger, json_t *topic,
    const char *key, json_int_t rowid, uint32_t mask, BOOL set);
```

### Reading: Iterators

For historical bulk reads over a key's records:

```c
json_t *tranger2_open_iterator(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *match_cond,         // from_rowid, to_rowid, from_t, to_t, backward, …
    tranger2_load_record_callback_t load_record_callback,
    const char *id,             // caller ID
    json_t *data                // user data for callback
);
int tranger2_close_iterator(json_t *tranger, json_t *iterator);
json_int_t tranger2_iterator_size(json_t *iterator);
json_t *tranger2_iterator_get_page(json_t *iterator, json_int_t from_rowid, size_t page_size);
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
json_t *tranger2_open_rt_mem(json_t *tranger, json_t *topic, const char *key,
    json_t *match_cond, tranger2_load_record_callback_t cb,
    const char *id, json_t *data);
int tranger2_close_rt_mem(json_t *tranger, json_t *rt_mem);

// Disk-backed (persistent, slower)
json_t *tranger2_open_rt_disk(json_t *tranger, json_t *topic, const char *key,
    json_t *match_cond, tranger2_load_record_callback_t cb,
    const char *id, json_t *data);
int tranger2_close_rt_disk(json_t *tranger, json_t *rt_disk);
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
// Node operations
json_t *treedb_create_node(json_t *tranger, const char *treedb_name,
    const char *topic_name, json_t *kw, hgobj src);
json_t *treedb_update_node(json_t *tranger, const char *treedb_name,
    const char *topic_name, json_t *kw, hgobj src);
int     treedb_delete_node(json_t *tranger, const char *treedb_name,
    const char *topic_name, json_t *kw, hgobj src);
json_t *treedb_get_node(json_t *tranger, const char *treedb_name,
    const char *topic_name, const char *id);
json_t *treedb_list_nodes(json_t *tranger, const char *treedb_name,
    const char *topic_name, json_t *jn_filter);

// Link operations
int treedb_link_nodes(json_t *tranger, const char *treedb_name,
    const char *hook, json_t *parent_node, json_t *child_node);
int treedb_unlink_nodes(json_t *tranger, const char *treedb_name,
    const char *hook, json_t *parent_node, json_t *child_node);
```

---

## ytls — TLS Abstraction

**Header:** `ytls/src/ytls.h`

Pluggable TLS layer supporting OpenSSL and mbed-TLS backends. Used by `C_TCP` for encrypted connections.

```c
// Initialize TLS context
hytls ytls_init(hgobj gobj, json_t *jn_config, BOOL server_side);
void  ytls_cleanup(hytls ytls);
const char *ytls_version(hytls ytls);

// Per-connection secure filter
hsskt ytls_new_secure_filter(hytls ytls, int (*on_handshake_done_cb)(hgobj, int, const char *),
    int (*on_clear_data_cb)(hgobj, gbuffer_t *),
    int (*on_encrypted_data_cb)(hgobj, gbuffer_t *), hgobj gobj);
void ytls_free_secure_filter(hytls ytls, hsskt sskt);

// TLS operations
int ytls_do_handshake(hytls ytls, hsskt sskt);  // 1=done, 0=in-progress, -1=fail
int ytls_encrypt_data(hytls ytls, hsskt sskt, gbuffer_t *gbuf);
int ytls_decrypt_data(hytls ytls, hsskt sskt, gbuffer_t *gbuf);
int ytls_shutdown(hytls ytls, hsskt sskt);
```

**Key config fields:** `library` ("openssl"/"mbedtls"), `ssl_certificate`, `ssl_certificate_key`, `ssl_trusted_certificate`, `ssl_ciphers`, `ssl_verify_depth`, `rx_buffer_size`, `trace`

---

## libjwt — JSON Web Tokens

**Header:** `libjwt/src/jwt.h`

JWT creation, parsing, and verification with support for HS256/384/512, RS256/384/512, ES256/384/512, PS256/384/512, EdDSA.

```c
// Decode (verify signature)
int jwt_decode(jwt_t **jwt, const char *token,
    const jwk_item_t *key);

// Build and sign
jwt_builder_t *jwt_builder_new(void);
int jwt_builder_setkey(jwt_builder_t *builder, jwt_alg_t alg, const jwk_item_t *key);
int jwt_builder_claim_add_str(jwt_builder_t *builder, const char *claim, const char *val);
int jwt_builder_claim_add_int(jwt_builder_t *builder, const char *claim, long long val);
char *jwt_builder_generate(jwt_builder_t *builder);
void jwt_builder_free(jwt_builder_t *builder);

// JWK sets (local or remote)
jwk_set_t *jwks_create(const char *jwks_json_str);
jwk_set_t *jwks_load_from_file(const char *file_path);
void jwks_free(jwk_set_t *jwks);
const jwk_item_t *jwks_get_by_kid(jwk_set_t *jwks, const char *kid);
```

---

## root-linux — Runtime GClasses

**Location:** `root-linux/src/`

33 GClasses providing network, protocol, storage, and system functionality. All registered via `yunetas_register()`.

### Registration

```c
#include "yunetas_register.h"

int yunetas_register(void);  // register all built-in GClasses
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
| `C_PROT_MODBUS_M` | Modbus RTU/TCP master |

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
./extrae.sh                 # extract source archives
./install-libs.sh           # build and install to outputs_ext/

# For musl/static builds:
./extrae-static.sh
./install-libs-static.sh
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

PRIVATE sdata_desc_t tattr_desc[] = {
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
        tattr_desc,
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
│   ├── extrae.sh / extrae-static.sh
│   ├── install-libs.sh / install-libs-static.sh
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
│   ├── c_prot_mqtt.c, c_prot_modbus_m.c
│   ├── msg_ievent.h / msg_ievent.c  ← inter-event wire format
│   ├── entry_point.h / entry_point.c
│   └── yunetas_register.h / .c
│
└── root-esp32/                      ← ESP32 embedded components
    ├── common_components/
    └── components/
```
