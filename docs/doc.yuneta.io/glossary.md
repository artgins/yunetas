# Glossary

Key terms and concepts of the Yuneta framework, sorted alphabetically.

---

**Action callback**
:   Function executed when an event fires in a given FSM state. Signature: `int (*)(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)`. See [GClass guide](guide/guide_gclass.md).

**Append-only log**
:   Timeranger2's fundamental storage model — records are only appended, never overwritten. Each record gets a monotonically increasing `g_rowid`. See [Timeranger2 guide](guide/guide_timeranger2.md).

(attribute)=
**Attribute**
:   A typed property of a gobj, defined in the GClass `attrs_table` via [SData](#glossary-sdata) descriptors. Attributes have a type, access flags, default value, and description. See [Attributes API](api/gobj/attrs.md).

**Authentication**
:   Verification of a user's identity through credentials (JWT tokens, user/password). See [Authorization guide](guide/guide_authz.md).

**Authorization**
:   Access control that determines whether an authenticated user may perform a specific action. Defined per GClass in the `authz_table`. See [Authorization guide](guide/guide_authz.md).

**Bottom gobj**
:   A designated child gobj that shares attributes and forwards events with its parent — Yuneta's alternative to class inheritance. See [Basic concepts](guide/basic_concepts.md).

**Command**
:   A named operation exposed by a gobj's `command_table`, executable from the control plane regardless of FSM state. See [Command parser guide](guide/parser_command.md).

**Composition**
:   Yuneta's object-reuse pattern: behavior is assembled by nesting gobjs (parent → child → bottom) and routing events between them, instead of using class inheritance.

(control-plane)=
**Control plane**
:   The built-in mechanism that exposes every yuno's commands and stats over a local socket or WebSocket. Interact with it using `ycommand`, `ybatch`, or `ystats`. See [Utilities](utilities.md).

**CRUDLU**
:   Extended CRUD operations for [TreeDB](#treedb): **C**reate, **R**ead, **U**pdate, **D**elete, **L**ink, **U**nlink — accounting for graph relationships between nodes.

(event)=
**Event**
:   A typed message (event name + JSON `kw` payload) sent between gobjs via `gobj_send_event()`. Events are the **only** way gobjs communicate. See [Events & State API](api/gobj/events_state.md).

**Event-driven architecture**
:   Yuneta's core design: nothing happens without an event. The [yev_loop](#yev-loop) processes I/O, timers, and signals, then propagates them as gobj events.

(fkey)=
**Fkey** (Foreign Key)
:   A field on a child [TreeDB](#treedb) node that stores a persistent reference to its parent, encoded as `"topic^parent_id^hook_name"`. Link/unlink operations save **only** the child's fkey.

(fsm)=
**FSM** (Finite State Machine)
:   The mechanism that governs a gobj's behavior: a set of states, valid events per state, action callbacks, and state transitions. Every gobj has one. See [GClass guide](guide/guide_gclass.md).

(glossary-gclass)=
**GClass** (GObject Class)
:   A blueprint defining the structure and behavior of gobjs: FSM (states + events), attributes, commands, authorization rules, and lifecycle methods (GMethods). See [GClass guide](guide/guide_gclass.md).

**GMethod**
:   A lifecycle or behavioral callback in the GClass `GMETHODS` table (`mt_create`, `mt_destroy`, `mt_start`, `mt_stop`, `mt_play`, `mt_pause`, etc.). See [GClass guide](guide/guide_gclass.md).

(glossary-gobj)=
**gobj** (GObject)
:   A runtime instance of a [GClass](#glossary-gclass): a modular, event-driven component with its own FSM, attributes, and position in the gobj tree. See [Basic concepts](guide/basic_concepts.md).

**g_rowid**
:   Global record identifier in timeranger2 — monotonically increasing counter for a given key within a topic. Never resets.

**GBuffer** (`gbuffer_t`)
:   Growable byte buffer used for binary I/O throughout the framework. See [GBuffer guide](guide/guide_gbuffer.md).

**hgobj**
:   Opaque handle to a gobj instance (`typedef void *hgobj`).

**hgclass**
:   Opaque handle to a registered GClass (`typedef void *hgclass`).

(hook)=
**Hook**
:   A field on a parent [TreeDB](#treedb) node that references child nodes. Hooks are in-memory relationships — persistence is through the child's [fkey](#fkey).

**Horizontal scaling**
:   Yuneta's scaling strategy: run one yuno per CPU core and communicate via inter-event messaging. No threads, no locks.

**i_rowid**
:   Row index within a key's md2 file in timeranger2 — monotonically increasing per key.

**Inter-event messaging**
:   RPC-like communication between yunos over the network. A local gobj subscribes to a remote service as if it were local. See [Inter-Event GClasses](api/gclass/ievent.md) and [Messaging API](api/runtime/msg_ievent.md).

(io-uring)=
**io_uring**
:   Modern Linux asynchronous I/O interface used by [yev_loop](#yev-loop) for all non-blocking operations (sockets, timers, signals, filesystem).

**JSON**
:   The **only** data format in Yuneta — used for events, messages, logs, stats, configuration, and persistence. All payloads are `json_t *` objects (Jansson library).

(kw)=
**kw** (keyword arguments)
:   A `json_t *` JSON object carrying the data payload of an event or command. Ownership semantics (owned/not-owned) are indicated in function signatures.

**kwid**
:   Library for advanced JSON manipulation: path-based access, filtering, cloning, matching, and comparison of JSON structures. See [kwid guide](guide/guide_kwid.md) and [kwid API](api/helpers/kwid.md).

**LMethod** (Local Method)
:   A private method on a GClass, invoked explicitly via `gobj_local_method()` — not part of the FSM dispatch.

(node)=
**Node**
:   A JSON object in [TreeDB](#treedb) identified by a primary key (`id`). Contains `__md_treedb__` metadata with `g_rowid`, `i_rowid`, `topic_name`, etc.

**Parent / Child**
:   Gobjs form a hierarchical tree. Every gobj has exactly one parent (except the root yuno). Parents create and manage their children.

**Persistent attribute**
:   An attribute with the `SDF_PERSIST` flag — automatically saved to and loaded from disk. See [Persistent attrs guide](guide/persistent_attrs.md).

**Publish / Subscribe**
:   Pattern where a gobj publishes output events to all subscribed gobjs via `gobj_publish_event()` / `gobj_subscribe_event()`. See [Publish API](api/gobj/publish.md).

**Pure child**
:   A gobj created with `gobj_flag_pure_child` — sends events directly to its parent without requiring explicit subscriptions.

**Realm**
:   A running yuno or a logically grouped set of yunos, identified by Role, Name, and Owner.

**Re-launch** (Watcher-Worker)
:   Yuneta's self-healing daemon mechanism. In daemon mode, a *watcher* process monitors the *worker* (the actual yuno) and automatically re-launches it on unexpected termination. No systemd required.

(glossary-sdata)=
**SData** (Structured Data)
:   The schema system used to define typed fields with metadata (type, flags, default, description) for attributes, commands, and database records. See [SData guide](guide/guide_sdata.md).

(service)=
**Service**
:   A gobj registered with `gobj_create_service()` that exposes commands and events for external interaction. Found by name via `gobj_find_service()`.

**Single-threaded**
:   Every yuno runs in a single thread — no locks, no mutexes. CPU-bound work blocks the entire event loop. Scale horizontally instead.

(snapshot)=
**Snapshot**
:   A point-in-time capture of [TreeDB](#treedb) state for backup/restore. Managed via `treedb_shoot_snap()` / `treedb_activate_snap()`.

(state)=
**State**
:   A named stage in a gobj's [FSM](#fsm). Predefined states: `ST_STOPPED`, `ST_IDLE`, `ST_DISCONNECTED`, `ST_CONNECTED`, `ST_OPENED`, `ST_CLOSED`.

**Stats**
:   Operational metrics exposed by every gobj (byte counters, message rates, connection counts, etc.). Attributes flagged `SDF_STATS` or `SDF_RSTATS`. Query with `gobj_stats()` or `ystats`.

**Subscriber**
:   A gobj that has subscribed to receive published events from another gobj.

(glossary-timeranger2)=
**Timeranger2**
:   Append-only time-series storage engine — the persistence primitive underlying [TreeDB](#treedb), queues, and message stores. See [Timeranger2 guide](guide/guide_timeranger2.md) and [Timeranger2 API](api/timeranger2/timeranger2.md).

(topic)=
**Topic**
:   A collection of records in timeranger2, organized by key with time-based indexing.

**Trace level**
:   A runtime diagnostic category that can be dynamically enabled/disabled per gobj or GClass. Defined in `s_user_trace_level` and controlled via the control plane.

(treedb)=
**TreeDB** (`tr_treedb`)
:   Graph memory database built on timeranger2. Nodes belong to topics and are linked via [hook](#hook)/[fkey](#fkey) relationships. See [TreeDB API](api/timeranger2/treedb.md).

(yev-loop)=
**yev_loop**
:   The event loop engine — drives all asynchronous I/O using Linux [io_uring](#io-uring). Manages sockets, timers, signals, and filesystem events. See [Event Loop guide](guide/guide_yev_loop.md) and [Event Loop API](api/yev_loop/yev_loop.md).

(glossary-yuno)=
**Yuno**
:   A deployable, single-threaded process composed of a hierarchical tree of gobjs. The root gobj is always a [C_YUNO](api/gclass/system.md#gclass-c-yuno). See [Basic concepts](guide/basic_concepts.md).
