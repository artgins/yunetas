# The gobj framework, in 30 minutes

This is the crash course on Yuneta's runtime. By the end you should know
what a gobj is, what a gclass is, how to declare one, how the framework
calls into it, how the runtime tree is laid out, and where the sharp
edges are.

> **Conceptual frame.** This document describes the **behavior plane**
> of Yuneta's typed-graph model. The information plane is in
> [`YUNO_TREEDB.md`](YUNO_TREEDB.md). The claim that both planes share
> one set of primitives — `gclass`/`topic`, `gobj`/`node`,
> subscription/`hook` — is laid out in
> [The Typed-Graph Model](../../../docs/doc.yuneta.io/philosophy/typed_graph_model.md).
> Read that first if you want to know *why* gobj and treedb look so
> similar before diving into either one.

Sibling to [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md), [`DEBUGGING.md`](DEBUGGING.md),
[`IPC.md`](IPC.md), [`REALMS.md`](REALMS.md), [`SCAFFOLDING.md`](SCAFFOLDING.md),
[`YUNO_AUTH.md`](YUNO_AUTH.md). Events and message dispatch are already covered in
detail in [`IPC.md`](IPC.md) — this doc only sketches the bits needed to
understand the lifecycle.

---

## 1. The two halves: gclass vs gobj

| Concept   | Role                                                                     |
|-----------|--------------------------------------------------------------------------|
| **gclass** | The *class* (in the OO sense). Static, registered once at process start. Declares attrs, FSM (states + events + actions), command table, methods. Examples: [`C_TIMER`](#gclass-c-timer), [`C_TCP_S`](#gclass-c-tcp-s), [`C_AUTH_BFF`](#gclass-c-auth-bff). |
| **gobj**   | A *runtime instance*. Created by `gobj_create*`, lives in a tree, holds a `priv` data pointer, has an FSM state, a `running`/`playing` flag, subscriptions, etc. |

A gclass declares `mt_*` framework methods (next section). A gobj is the
object on which those methods get called. The gclass struct lives in
read-only data; one gobj instance per `gobj_create*` call lives on the
heap with its own `priv`.

In source files, by convention:

```
kernel/c/root-linux/src/c_timer.c    ← the gclass
                       c_timer.h     ← public registration entry
```

A gclass file always exposes exactly two public symbols (CLAUDE.md hard
rule, `feedback_gclass_public_interface`):

```c
GOBJ_DECLARE_GCLASS(C_TIMER);
PUBLIC int register_c_timer(void);
```

Everything else (commands, attrs, events, lmethods, stats) is reached
through the standard framework APIs.

---

## 2. The shape of a gclass file

Every gclass conforms to the `gclass_service` or `gclass_child` skeleton
(see [`SCAFFOLDING.md`](SCAFFOLDING.md) §6). The banner order is **not
optional** (CLAUDE.md "GClass templates and skeletons"). At a glance, in
the order they appear in a file:

```
        ┌─────────────────────────────────────┐
        │           PRIVATE DATA              │ ← typedef struct PRIVATE_DATA { … } PRIVATE_DATA;
        │                                     │   Per-instance C state held in priv pointer.
        └─────────────────────────────────────┘
        ┌─────────────────────────────────────┐
        │             ATTRIBUTES              │ ← const sdata_desc_t attrs_table[] = { … };
        │                                     │   Typed schema of every attr the gclass exposes.
        └─────────────────────────────────────┘
        ┌─────────────────────────────────────┐
        │       FRAMEWORK METHODS (mt_*)      │ ← mt_create, mt_start, mt_stop, mt_destroy, …
        └─────────────────────────────────────┘
        ┌─────────────────────────────────────┐
        │              COMMANDS               │ ← const sdata_desc_t command_table[] = { … };
        │                                     │   cmd_xxx() handlers
        └─────────────────────────────────────┘
        ┌─────────────────────────────────────┐
        │            LOCAL METHODS            │ ← const sdata_desc_t lmt_table[] = { … };
        │                                     │   lmt_xxx() handlers
        └─────────────────────────────────────┘
        ┌─────────────────────────────────────┐
        │              ACTIONS                │ ← ac_xxx() FSM action functions
        └─────────────────────────────────────┘
        ┌─────────────────────────────────────┐
        │     EVENTS / STATES / GCLASS REG    │ ← event_types[], states[], create_gclass()
        └─────────────────────────────────────┘
```

Empty sections still get their banner. Don't reorder.

---

## 3. The framework method table (`GMETHODS`)

Declared in [`gobj.h`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.h) as `gobj_method_t`. The framework calls
these on your gclass at fixed points in the lifecycle. None is required —
a gclass with **zero** methods is technically valid (it would just be a
data bag). In practice you always implement `mt_create` and `mt_destroy`
at minimum, plus `mt_start`/`mt_stop` if you have any runtime resources.

Most useful ones, in roughly the order you'll write them:

| Method                       | When the framework calls it                                       | What you typically do                                   |
|------------------------------|-------------------------------------------------------------------|---------------------------------------------------------|
| `mt_create(gobj)`            | After the gobj is allocated, attrs initialised, but **before** mt_child_added on the parent | Cache attrs into `priv`, set up the CHILD/SERVICE subscription block (see [`SCAFFOLDING.md`](SCAFFOLDING.md) §6) |
| `mt_create2(gobj, kw)`       | Alt to `mt_create` when you need the raw `kw` of the creation call | Same as above, plus `kw`-driven config                  |
| `mt_destroy(gobj)`           | After the gobj is stopped, paused, and **all children are already destroyed** | Free resources not owned by children                    |
| `mt_start(gobj)`             | `gobj_start()` flips `running=TRUE` ([gobj.c:4311](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c#L4311)) then calls this  | Start timers, subscribe to peers, open sockets          |
| `mt_stop(gobj)`              | `gobj_stop()` flips `running=FALSE` then calls this                | Stop timers, unsubscribe, close sockets                 |
| `mt_play(gobj)` / `mt_pause(gobj)` | `gobj_play()` / `gobj_pause()`                              | Gate input processing (see [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md) §4.4) |
| `mt_writing(gobj, name)`     | After `gobj_write_*_attr()` succeeds                     | Sync `priv->field` from the attr; validate; react       |
| `mt_reading(gobj, name)`     | Inside `gobj_read_*_attr()`                              | Return a *computed* value — **required for `SDF_RSTATS` counters** |
| `mt_child_added` / `mt_child_removed` | When children are created/destroyed                       | Track child references, wire subscriptions              |
| `mt_subscription_added` / `mt_subscription_deleted` | When someone subscribes to your events            | Send initial state, clean up per-subscriber resources   |
| `mt_inject_event(gobj, ev, kw, src)` | When `gobj_send_event` finds no row in the state table     | The escape hatch — handle the event yourself, or fail   |
| `mt_command_parser`          | When a command is invoked but **not** in `command_table`           | Custom command dispatch (rare; only the agent uses this for `command-yuno` forwarding) |
| `mt_authz_checker`           | Inside [`gobj_user_has_authz`](#gobj_user_has_authz) if installed ([gobj.c:9400](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c#L9400))       | Per-gclass authz hook — see [`YUNO_AUTH.md`](YUNO_AUTH.md) §4.3   |

The full table (28+ slots — [`mt_create_resource`](#mt_create_resource), [`mt_save_resource`](#mt_save_resource),
[`mt_create_node`](#mt_create_node), [`mt_link_nodes`](#mt_link_nodes), etc.) is in [`gobj.h`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.h). Most
gclasses use < 10 slots.

---

## 4. The lifecycle, step by step

```
                                gobj_destroy()
                              ┌─────────────────┐
                              │                 │
                       ┌──────▼────────┐    ┌───▼─────────┐
   gobj_create() ────► │   CREATED     │    │  DESTROYED  │ ← terminal
                       │ obflag_created│    └─────────────┘
                       └──────┬────────┘
                              │ gobj_start() or gobj_start_tree()
                              ▼
                       ┌────────────────┐
                       │   RUNNING      │ ← mt_start() ran
                       │ running=TRUE   │
                       └──────┬─────────┘
                              │
                              │ gobj_play() / gobj_pause() — orthogonal
                              │
                              ▼
                       ┌────────────────┐
                       │   PLAYING      │ ← mt_play() ran
                       │ playing=TRUE   │
                       └──────┬─────────┘
                              │ gobj_stop() or gobj_stop_tree()
                              ▼
                       ┌────────────────┐
                       │   STOPPED      │ ← mt_stop() ran
                       │ running=FALSE  │
                       └────────────────┘
```

Important code points:

- `gobj_create()` ([`gobj.c:1804`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c#L1804)): allocate, init attrs, set
  `obflag_created`, call `mt_create()`, then call parent's
  `mt_child_added()`.
- `gobj_start()` ([`gobj.c:4311`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c#L4311)): set `running=TRUE`
  **before** calling `mt_start()`. Inside your `mt_start`,
  `gobj_is_running(gobj)` already returns `TRUE`.
- `gobj_stop()` ([`gobj.c:4464`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c#L4464)): set `running=FALSE`
  before calling `mt_stop()`. Symmetric.
- `gobj_destroy()` ([`gobj.c:2272`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c#L2272)): pauses + stops if needed,
  unsubscribes the gobj from everyone, destroys all children, **then**
  calls `mt_destroy()`. That ordering is why `mt_destroy` can safely
  touch resources without checking child state.

### 4.1 `gobj_start_tree` vs `gobj_start`

`gobj_start_tree(gobj)` ([`gobj.c:4429`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c#L4429)) walks the gobj's subtree and
starts every child too — **except** ones marked with the
`gcflag_manual_start` gclass flag. Most CHILD-pattern gclasses do not
set that flag, so they get started automatically.

**The `gobj_create_pure_child` gotcha** (memory
`feedback_gobj_js_gotchas`):
`gobj_create_pure_child()` marks the new gobj with
`gobj_flag_pure_child`, and some constructors also imply
`gcflag_manual_start`. If your pure child does nothing after creation,
you almost certainly forgot to call `gobj_start(child)` explicitly. Or
use `gobj_start_tree(parent)` if you want the whole subtree started in
one shot.

### 4.2 `gobj_create*` flavours

[`gobj.h`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.h):

| Constructor                        | What you get                                                 |
|------------------------------------|--------------------------------------------------------------|
| `gobj_create(name, gclass, kw, parent)`           | Plain child. Standard case.            |
| `gobj_create2(name, gclass, kw, parent, flag)`    | Same + explicit `gobj_flag_*`.         |
| `gobj_create_yuno(name, gclass, kw)`              | The root. One per process. Sets `gobj_flag_yuno`. |
| `gobj_create_service(name, gclass, kw, parent)`   | A service — discoverable by name, autostartable. |
| `gobj_create_default_service(name, gclass, kw, parent)` | Like `_service` but the yuno calls `gobj_play()` later. |
| `gobj_create_volatil(name, gclass, kw, parent)`   | Temp child — destroyed when its parent is. |
| `gobj_create_pure_child(name, gclass, kw, parent)`| CHILD-pattern child. **Needs explicit `gobj_start`** (or `gobj_start_tree(parent)`). |

When in doubt: services for things addressable by name from other
yunos/the SPA, pure children for protocol stacks and per-connection
helpers, plain children for everything else.

---

## 5. Attributes via SData

A gclass declares its attributes in a `const sdata_desc_t attrs_table[]`
array, terminated by `SDATA_END()`. Each row is a typed schema:

```c
SDATA(type, name, flag, default_value, description)
```

Three things to know:

### 5.1 The types ([`gobj.h`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.h))

| `DTP_*`         | Concrete C / JSON type                |
|-----------------|---------------------------------------|
| `DTP_STRING`    | `const char *`                        |
| `DTP_INTEGER`   | [`json_int_t`](https://jansson.readthedocs.io/en/latest/apiref.html#c.json_int_t) (64-bit)                 |
| `DTP_BOOLEAN`   | `BOOL` (`0` / `1`)                    |
| `DTP_REAL`      | `double`                              |
| `DTP_JSON`      | `json_t *` (any JSON value)           |
| `DTP_DICT`      | `json_t *` (JSON object)              |
| `DTP_LIST`      | `json_t *` (JSON array)               |
| `DTP_POINTER`   | `void *` (opaque)                     |
| `DTP_SCHEMA`    | Sub-schema (commands' parameters)     |

### 5.2 The flags ([`gobj.h`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.h))

The ones that matter most often:

| Flag             | Effect                                                                |
|------------------|-----------------------------------------------------------------------|
| `SDF_RD`         | Reads allowed; writes denied.                                          |
| `SDF_WR`         | Writes allowed (implies `SDF_RD`).                                    |
| `SDF_REQUIRED`   | Must not be null at creation time.                                    |
| `SDF_PERSIST`    | Survives a yuno restart. Loaded on `mt_create` if persistence backend is installed. |
| `SDF_VOLATIL`    | Set by the framework or by other gclasses on this gobj. Read-only from user code. |
| `SDF_STATS`      | Treated as a statistic for stats output.                              |
| `SDF_RSTATS`     | Resettable stat — **requires `mt_reading` wired** to compute on read. See `feedback_mt_reading_for_rstats`. |
| `SDF_PSTATS`     | Persistent stat (combination of `SDF_PERSIST` + stat semantics).      |
| `SDF_DEPRECATED` | Logged-warning attribute, still accepted (see `auth_bff`'s `idp_url`). |
| `SDF_AUTHZ_R` / `SDF_AUTHZ_W` / `SDF_AUTHZ_X` / `SDF_AUTHZ_S` / `SDF_AUTHZ_RS` | Demand the matching authz. ⚠️ Currently **not enforced** for commands (see [`YUNO_AUTH.md`](YUNO_AUTH.md) §4.5). |

### 5.3 The R/W API

[`gobj.h`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.h). Typed accessors:

```c
const char *  gobj_read_str_attr     (hgobj gobj, const char *name);
json_int_t    gobj_read_integer_attr (hgobj gobj, const char *name);
BOOL          gobj_read_bool_attr    (hgobj gobj, const char *name);
double        gobj_read_real_attr    (hgobj gobj, const char *name);
json_t       *gobj_read_json_attr    (hgobj gobj, const char *name);   // not owned
void         *gobj_read_pointer_attr (hgobj gobj, const char *name);

int           gobj_write_str_attr    (hgobj gobj, const char *name, const char *value);
int           gobj_write_integer_attr(hgobj gobj, const char *name, json_int_t value);
…
```

Plus bulk APIs:

- `gobj_read_attrs(gobj, include_flag, src)` ([`gobj.c:3546`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c#L3546)) — returns
  a JSON object with every attr matching `include_flag`. Recently
  patched to route through `mt_reading` (memory
  `project_gobj_read_attrs_fix`,
  2026-05-22) so `SDF_RSTATS` counters now show their live values in
  bulk reads. Same fix mirrored in the JS side.
- `gobj_write_attrs(gobj, kw, include_flag, src)` ([`gobj.c:3622`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c#L3622)) —
  bulk write.

### 5.4 `SDF_PERSIST` storage

The framework hands `SDF_PERSIST` attrs to a pluggable backend
registered at startup. The default backend writes them under the yuno's
data directory. APIs ([`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c)):

- `gobj_load_persistent_attrs(gobj, jn_attrs)` — invoked automatically
  on service-flavour creates.
- `gobj_save_persistent_attrs(gobj, jn_attrs)` — call after mutating a
  `SDF_PERSIST` attr you want to checkpoint immediately.

Example from [`c_yuno.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/root-linux/src/c_yuno.c):

```c
SDATA(DTP_STRING,  "url_udp_log", SDF_PERSIST, "",  "UDP Log url"),
SDATA(DTP_STRING,  "process",     SDF_RD,      "",  "Process name"),
SDATA(DTP_STRING,  "hostname",    SDF_PERSIST, "",  "Hostname"),
SDATA(DTP_STRING,  "__username__",SDF_RD,      "",  "Username"),
SDATA(DTP_INTEGER, "pid",         SDF_RD,      "",  "pid"),
SDATA(DTP_STRING,  "node_uuid",   SDF_RD,      "",  "uuid of node"),
SDATA(DTP_STRING,  "realm_id",    SDF_RD,      "",  "Realm id"),
```

---

## 6. The runtime tree

A process is one yuno = one root gobj = `__yuno__`. Everything else
descends from it. The structure looks like this in production:

![The gobj runtime tree rooted at __yuno__: children authz (C_AUTHZ), the default service, gates (C_TCP_S, C_PROT_HTTP_SR, C_WEBSOCKET, C_IEVENT_SRV) and helpers; gates own per-connection pure children. Below, the bottom chain: a gobj resolves a missing attribute by walking down its bottom links.](../../../docs/doc.yuneta.io/_static/gobj_tree.svg)

The same tree in text:

```
            __yuno__   (gobj_flag_yuno, C_YUNO instance)
                │
       ┌────────┼────────┬────────────────┐
       │        │        │                │
       ▼        ▼        ▼                ▼
     authz   default   gates           helpers
   (service)  service  (e.g. C_TCP_S    (timers, watchdogs,
   C_AUTHZ   (your    + C_PROT_HTTP_SR  internal services)
              app)     + C_PROT_WEBSOCKET
                       + C_IEVENT_SRV)
                       │
                       └─ per-connection clisrv gobjs (pure children)
```

Navigation helpers ([`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c)):

| Function                            | Purpose                                                 |
|-------------------------------------|---------------------------------------------------------|
| `gobj_yuno()`                       | The root gobj. `__yuno__` global.                       |
| `gobj_parent(gobj)`                 | Immediate parent.                                       |
| `gobj_child_by_name(gobj, name)`    | First child with that name.                             |
| `gobj_child_by_index(gobj, idx)`    | 1-based index.                                          |
| `gobj_first_child(gobj)` / `gobj_next_child(child)` | Iterate linearly.                       |
| `gobj_child_size(gobj)`             | Count.                                                  |
| `gobj_walk_gobj_children(gobj, walk_type, cb, ud, ud2, ud3)` | Tree walk with callback.       |
| `gobj_find_gobj(path)`              | Lookup by full path, e.g. `"__yuno__\`auth\`bff"`.      |
| `gobj_bottom_gobj(gobj)` / `gobj_set_bottom_gobj` | The downward layering pointer (see [`IPC.md`](IPC.md) §6.5). |

Identification helpers ([`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c)):

```c
const char *gobj_name(gobj);                    // instance name
const char *gobj_gclass_name(gobj);             // gclass name ("C_TIMER")
const char *gobj_short_name(gobj);              // "gclass^name"
const char *gobj_full_name(gobj);               // full path
const char *gobj_yuno_role_plus_name();         // current yuno's role+name (used in command response prefixes — see feedback_build_command_response_yuno_prefix)
```

Predicates ([`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c)):

```c
BOOL gobj_is_running(gobj);
BOOL gobj_is_playing(gobj);
BOOL gobj_is_disabled(gobj);
BOOL gobj_is_service(gobj);
BOOL gobj_is_pure_child(gobj);
```

### 6.1 The service registry

`__jn_services__` (a global dict at [`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c)) maps service name →
gobj pointer, case-insensitively. Three special names always resolve:

| Name                    | Resolves to                                            |
|-------------------------|--------------------------------------------------------|
| `__yuno__` / `__root__` | The root gobj (`gobj_yuno()`).                         |
| `__default_service__`   | The yuno's default service (the first `gobj_create_default_service`). |

Public API:

```c
hgobj gobj_register_service(hgobj gobj, const char *service_name);
hgobj gobj_find_service(const char *service_name, BOOL verbose);
hgobj gobj_find_service_by_gclass(const char *gclass_name, BOOL verbose);
```

This is also how the ievent layer routes a remote command call to the
right local gobj — see [`IPC.md`](IPC.md) §4.5.

### 6.2 The `priv` pointer

Every gobj has a `void *priv` for the gclass's private C state. Access
with `gobj_priv_data(gobj)`. Pattern at the top of every action:

```c
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    …
}
```

You don't have to allocate it — the framework does, sized by the
`priv_size` argument to `create_gclass()`.

### 6.3 The `obflag` lifecycle bits

[`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c):

| Bit                  | Meaning                                                  |
|----------------------|----------------------------------------------------------|
| `obflag_created`     | `mt_create` returned. Object usable.                     |
| `obflag_destroying`  | `gobj_destroy` started. Don't subscribe / send.          |
| `obflag_destroyed`   | `mt_destroy` returned. Object inert.                     |

Check with `gobj->obflag & obflag_destroyed` if you're holding a raw
pointer and need to be safe — but the better answer is *don't hold raw
pointers across lifecycle boundaries*. See §8.5.

---

## 7. Worked example: [`c_timer.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/root-linux/src/c_timer.c)

The minimal canonical gclass per CLAUDE.md. 426 lines, single state, two
events, one action. Open the file alongside this — every Yuneta C
developer should be able to read it cold.

### 7.1 What it does

[`c_timer.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/root-linux/src/c_timer.c) is a periodic / one-shot timer. The subscriber sets the
`msec` and `periodic` attrs, then starts it; the gclass listens to the
yuno's global `EV_TIMEOUT_PERIODIC` heartbeat and republishes its own
`EV_TIMEOUT` (one-shot, then auto-stop) or `EV_TIMEOUT_PERIODIC` (every
`msec`).

### 7.2 `mt_create`

```c
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    } else if(gobj_is_pure_child(gobj)) {
        subscriber = gobj_parent(gobj);
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    SET_PRIV(periodic, gobj_read_bool_attr)
    SET_PRIV(msec,     gobj_read_integer_attr)
}
```

The first block is the verbatim **SERVICE** subscription pattern from
[`SCAFFOLDING.md`](SCAFFOLDING.md) §6.1. The second block caches attrs
into `priv` so the runtime hot path doesn't need to walk the attr table.

### 7.3 `mt_writing`

Triggers when someone calls `gobj_write_bool_attr(gobj, "periodic", …)`
or `gobj_write_integer_attr(gobj, "msec", …)`. Refreshes the priv
cache:

```c
PRIVATE void mt_writing(hgobj gobj, const char *name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    IF_EQ_SET_PRIV(periodic,  gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(msec,    gobj_read_integer_attr)
    END_EQ_SET_PRIV()
}
```

This is the **cache-coherent** pattern. Whenever your `priv` holds
copies of attrs for performance, `mt_writing` is where you keep them in
sync.

### 7.4 `mt_start` / `mt_stop`

Subscribe to the global heartbeat and start counting:

```c
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gobj_subscribe_event(gobj_yuno(), EV_TIMEOUT_PERIODIC, 0, gobj);
    if(priv->msec > 0) {
        priv->t_flush = start_msectimer(priv->msec);
    }
    return 0;
}
```

`mt_stop` is the mirror: clear the timer, unsubscribe.

### 7.5 The state table

```c
PRIVATE ev_action_t st_idle[] = {
    {EV_TIMEOUT_PERIODIC, ac_timeout, 0},
    {0,                   0,          0}
};
PRIVATE states_t states[] = {
    {ST_IDLE, st_idle},
    {0,       0}
};
PRIVATE event_type_t event_types[] = {
    {EV_TIMEOUT,           EVF_OUTPUT_EVENT},
    {EV_TIMEOUT_PERIODIC,  EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
    {0, 0}
};
```

Single state. Single transition. Two output events. The
`EVF_NO_WARN_SUBS` on `EV_TIMEOUT_PERIODIC` is the *"missing subscriber
is not a bug"* annotation (CLAUDE.md, [`IPC.md`](IPC.md) §3.3).

That's the entire FSM machinery of a real, shipping gclass.

---

## 8. Sharp edges

### 8.1 The state-before-action HACK

Inside an action, `gobj_current_state(dst)` returns the **new** state,
not the one that fired the event ([`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c)). Documented in
detail in [`IPC.md`](IPC.md) §3.2 — re-read it before writing your
first FSM with non-trivial state transitions.

### 8.2 `kw` is owned by the callee

Every event-carrying API consumes one reference (gobj.h, [`IPC.md`](IPC.md)
§2.3). If you need to keep `kw` around, `KW_INCREF` first. Forgetting
this leaks JSON memory. Double-decref crashes.

### 8.3 `mt_destroy` runs after children are destroyed

([`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c)). You can free shared resources without checking
child state. **Conversely**: don't free a resource in `mt_destroy` that
a child might still be using — children get `mt_destroy`'d first, so
they release their use of the resource before the parent's `mt_destroy`
runs.

### 8.4 SDF_RSTATS counters need `mt_reading` wired

The framework returns the **stored** value of an attr unless the gclass
provides `mt_reading`. For counters that auto-reset on read (`SDF_RSTATS`),
this means you'll always see `0` if you forgot the method. See
`feedback_mt_reading_for_rstats`
and `project_gobj_read_attrs_fix`
for the bulk-read variant (fixed 2026-05-22).

### 8.5 Prefer explicit lifecycle over framework-deferred destroy

Memory
`feedback_explicit_lifecycle`:
*Yuneta is not a JVM*. Prefer **static child ownership** (the parent
keeps a `hgobj` pointer in `priv`, destroys it itself at the right
moment) over framework-level deferred-destroy mechanisms. Cleaner, no
hidden ordering dependencies.

### 8.6 `gobj_create_pure_child` does not auto-start

([`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c), memory
`feedback_gobj_js_gotchas`).
You must call `gobj_start(child)` or rely on `gobj_start_tree(parent)`.

### 8.7 No sync read inside a `yev_loop` callback

Memory
`feedback_no_sync_read_in_yev_cb`:
interactive sub-modes (completion prompts, etc.) must use an ncurses
overlay + an in-mode flag + FSM transitions, **never** a blocking
`read(STDIN_FILENO)` from inside a `yev_loop` callback. Same applies to
any blocking syscall — the loop is single-threaded; blocking it kills
the yuno.

### 8.8 No sync stop in a published-event callback

Memory
`feedback_no_sync_stop_in_pub_callback`:
`gobj_stop_tree(publisher)` from inside an `EV_ON_*` subscriber re-enters
the publisher mid-iteration. Defer with a `dying` flag + a death timer.

### 8.9 A gclass `.h` exposes only `register_*` + `GOBJ_DECLARE_GCLASS`

CLAUDE.md hard rule, memory
`feedback_gclass_public_interface`.
Every interaction with a gclass goes through attrs / commands / events /
lmethods / stats — never through a direct API exposed in the `.h`.

### 8.10 No raw `malloc` / `free` in Yuneta C code

CLAUDE.md hard rule. Use `gbmem_*`. Mixing libc allocations with
gbmem corrupts the heap. Jansson is already routed through `gbmem_*`
internally — `json_*` is fine.

### 8.11 No static `json_t *` caches

Memory
`feedback_no_static_json_cache`:
`static json_t *cached` leaks at [`gobj_end`](#gobj_end) under
`CONFIG_DEBUG_TRACK_MEMORY`. Parse on each call or hand the cache to a
gobj that has a lifecycle.

### 8.12 Always braces

CLAUDE.md hard rule. Every `if` / `for` / `while` body is multi-line
braced, even single-statement ones. See `CLAUDE.md` for the rationale.

### 8.13 `gobj_publish_event` is synchronous — guard post-publish state resets

`gobj_publish_event` delivers to every subscriber **synchronously**;
when the call returns, every subscriber has fully processed the event,
including any cascades they triggered. Under [io_uring](#io-uring) this is
unforgiving: a single publish can travel up the chain, react with
`EV_DROP` going down, hit `C_TCP::set_disconnected`, which publishes
`EV_DISCONNECTED` back up, etc. — your gobj can re-enter `ac_disconnected`
and end up in `ST_DISCONNECTED` **before** the line after your publish
runs.

Concretely, this pattern in a frame-oriented protocol is wrong:

```c
gobj_publish_event(gobj, EV_ON_MESSAGE, kw);
start_wait_frame_header(gobj);      // ← bug: clobbers ST_DISCONNECTED
```

It leaves the protocol "connected" without an underlying TCP, and the
next reconnect's `EV_CONNECTED` lands in `ST_WAIT_FRAME_HEADER` instead
of `ST_DISCONNECTED` → "Event NOT DEFINED in state" cascade.

Canonical guard (four of the five frame-oriented gclasses use it):

```c
gobj_publish_event(gobj, EV_ON_MESSAGE, kw);
if(gobj_current_state(gobj) != ST_DISCONNECTED) {
    start_wait_frame_header(gobj);
}
```

[`c_prot_mqtt2.c`](https://github.com/artgins/yunetas/blob/7.5.2/modules/c/mqtt/src/c_prot_mqtt2.c) uses the more defensive
`gobj_is_running(gobj) && !gobj_in_this_state(gobj, ST_DISCONNECTED)`
form (also handles the gobj-stopped case). Either is fine.

Sites with this guard today: [`c_websocket.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/root-linux/src/c_websocket.c), [`c_prot_tcp4h.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/root-linux/src/c_prot_tcp4h.c),
[`c_prot_mqtt.c`](https://github.com/artgins/yunetas/blob/7.5.2/modules/c/mqtt/src/c_prot_mqtt.c), [`c_prot_mqtt2.c`](https://github.com/artgins/yunetas/blob/7.5.2/modules/c/mqtt/src/c_prot_mqtt2.c). Fixed in commits
`635b06a41` and `855527770` (2026-05-24).

**Intentionally unguarded:** [`c_prot_modbus_m.c`](https://github.com/artgins/yunetas/blob/7.5.2/modules/c/modbus/src/c_prot_modbus_m.c) (`ac_rx_data`). The guard was applied in `855527770` and then reverted in
`54a2624f8` (2026-05-25): the modbus master flow is polled-master, the
`EV_ON_MESSAGE` publish does not have an upstream authz NAK path that
re-enters `ac_disconnected`, so the cascade pattern does not arise.
Leave the `RESET_MACHINE()` + `gobj_change_state(ST_CONNECTED)` pair
unguarded there; do not re-add the guard from a "consistency" grep
without first reproducing an actual cascade.

**Rule of thumb:** after any synchronous `gobj_publish_event` that
could plausibly produce a disconnect upstream (`EV_ON_MESSAGE`,
`EV_ON_ID`, etc.), read the current state before mutating it.

---

## 9. Recipes

### 9.1 Write a new SERVICE gclass

```bash
yuno-skeleton gclass_service my_service
# → c_my_service.c, c_my_service.h
```

In `c_my_service.h`: only `GOBJ_DECLARE_GCLASS(C_MY_SERVICE)` and
`register_c_my_service()`. Edit the `.c`:

1. Add attrs to `attrs_table[]` (use the right types and flags).
2. Optionally add commands to `command_table[]`.
3. Add `mt_*` methods you need. Minimum: `mt_create` (verbatim from
   template), `mt_destroy` (free your resources), `mt_start`/`mt_stop` if
   you have runtime state.
4. Declare events in `event_types[]`, transitions in per-state
   `ev_action_t[]`, states in `states[]`.
5. In your yuno's `main.c`, call `register_c_my_service()` **before**
   the first gobj that uses it.

### 9.2 Read and write an attr

```c
// reading
const char *url = gobj_read_str_attr(gobj, "url");
json_int_t  msec = gobj_read_integer_attr(gobj, "msec");

// writing
gobj_write_str_attr(gobj, "url",    new_url);
gobj_write_integer_attr(gobj, "msec", new_msec);

// for persistent attrs, checkpoint manually if you want
// the change to survive a hard crash before next stop:
json_t *to_save = json_pack("[s]", "url");
gobj_save_persistent_attrs(gobj, to_save);
JSON_DECREF(to_save);
```

### 9.3 Find a service from another gobj

```c
hgobj authz = gobj_find_service("authz", TRUE);  // verbose: log if not found
if(!authz) {
    return -1;
}
gobj_command(authz, "user-authzs", json_pack("{s:s}", "user_id", "alice"), gobj);
```

### 9.4 Walk a subtree

```c
PRIVATE int my_walker(hgobj child, void *ud, void *ud2, void *ud3)
{
    gobj_log_info(child, 0, "msgset", "%s", "BROWSE", "msg", "%s",
                  gobj_short_name(child), NULL);
    return 0; // keep walking; return non-0 to stop
}

gobj_walk_gobj_children(gobj_yuno(), WALK_FIRST2LAST, my_walker, NULL, NULL, NULL);
```

### 9.5 Add a periodic timer

```c
// in your mt_create:
hgobj timer = gobj_create_default_service("my_timer", C_TIMER, NULL, gobj);
gobj_write_integer_attr(timer, "msec",     5000);
gobj_write_bool_attr   (timer, "periodic", TRUE);
gobj_subscribe_event(timer, EV_TIMEOUT_PERIODIC, NULL, gobj);
gobj_start(timer);

// in your mt_stop:
hgobj timer = gobj_find_child(gobj, json_pack("{s:s}", "__gobj_name__", "my_timer"));
if(timer) {
    gobj_stop(timer);
}

// in your states' ev_action_list:
{EV_TIMEOUT_PERIODIC, ac_tick, 0},
```

---

## 10. Code pointers

| What                                              | Where                                                              |
|---------------------------------------------------|--------------------------------------------------------------------|
| Public API surface                                | [`kernel/c/gobj-c/src/gobj.h`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.h) (2081 lines)                          |
| Runtime                                           | [`kernel/c/gobj-c/src/gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c) (~12k lines)                          |
| `GMETHODS` struct                                 | [`gobj.h`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.h)                                                   |
| `gobj_create` family                              | [`gobj.h`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.h)                                                   |
| Lifecycle implementation                          | [`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c) (create, start/stop, destroy)                     |
| `mt_writing` / `mt_reading` call sites            | [`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c)                                    |
| `obflag` flags                                    | [`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c)                                                     |
| Attr R/W API                                      | [`gobj.h`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.h)                                                 |
| Bulk attr API + `mt_reading` route                | [`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c)                                                |
| Persistent attr load/save                         | [`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c)                                                 |
| SData types (`DTP_*`)                             | [`gobj.h`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.h)                                                     |
| SData flags (`SDF_*`)                             | [`gobj.h`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.h)                                                    |
| Service registry                                  | [`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c)                                            |
| Tree navigation                                   | [`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c) (children walks, predicates)                    |
| `gobj_yuno()` / `__yuno__`                        | [`gobj.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/gobj-c/src/gobj.c)                                            |
| Canonical minimal gclass                          | [`kernel/c/root-linux/src/c_timer.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/root-linux/src/c_timer.c) (426 lines)                    |
| Canonical large gclass                            | [`kernel/c/root-linux/src/c_yuno.c`](https://github.com/artgins/yunetas/blob/7.5.2/kernel/c/root-linux/src/c_yuno.c), [`yunos/c/yuno_agent/src/c_agent.c`](https://github.com/artgins/yunetas/blob/7.5.2/yunos/c/yuno_agent/src/c_agent.c) |
| Banner / template conventions                     | `utils/c/yuno-skeleton/skeletons/`, [`SCAFFOLDING.md`](SCAFFOLDING.md) |
