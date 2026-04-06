# Design Principles

This page answers the question **"why is Yuneta built the way it is?"**.
It is the engineering counterpart to the more evocative
[Inspiration](philosophy.md) page and to the concrete
[Domain Model](domain_model.md). Read this first if you are evaluating
the framework.

:::{note} Key terms used on this page
Just enough vocabulary to read the rest of the document. Full
definitions live in [Basic Concepts](../guide/basic_concepts.md).

- **yuno** — a Yuneta process. One single-threaded binary holding
  a tree of gobjs plus the runtime that drives them.
- **gobj** — the runtime instance: an event-driven object with its
  own finite state machine and attributes.
- **GClass** — the *template* a gobj is instantiated from. Declares
  states, events, attribute schema, commands, and action callbacks.
  There is no inheritance.
- **event + `kw`** — the only way gobjs talk to each other. An event
  is a typed name; `kw` is its JSON key-value payload (`json_t *`).
- **SData** — the typed schema used for a GClass's attributes and
  commands. Carries type, default, persistence and authorization
  flags in one declaration.
- **bottom gobj** — the optional delegate a gobj points at to share
  attributes and forward events (Yuneta's alternative to inheritance).
- **timeranger2 / TreeDB** — the append-only time-series store and
  the in-memory graph view built on top of it.
:::

## The problem Yuneta was built to solve

Yuneta exists to build **long-lived, message-driven services** that run
on Linux — typically on bare metal, at the edge, in embedded boxes, or
inside IoT gateways — and that need to:

- ingest data from **many devices and protocols** at the same time,
- route, transform, adapt, and **forward messages** between systems,
- optionally **persist everything** (messages, state, configuration,
  history) as time-series and graphs,
- expose a **control plane** so operators and tools can inspect and
  steer the running process without restarting it,
- and **stay up for months** with predictable memory and CPU
  behaviour.

Most frameworks optimise for one of those axes. Yuneta is the result
of doing all of them, with the same conceptual core (GClass, gobj,
events, FSM, tree-of-objects, append-only persistence), iterated in
production since the early 2000s.

## The decisions that define Yuneta

The rest of this document is one section per design decision. For each,
you get the **what**, the **why**, and the **trade-off**.

### 1. Events and finite state machines are the *only* way gobjs interact

Every interaction between two gobjs is an **event**: a typed name plus
a `json_t *kw` payload. Every gobj is a **finite state machine**: its
GClass declares states, the events accepted in each state, and the
action callback that runs when an accepted event arrives.

**Why.** FSMs make behaviour enumerable. You can draw the state
diagram, reason about unreachable code, replay an event log, and write
fuzz tests that drive the machine through every transition. Callbacks
and ad-hoc flags scale badly in long-lived services; state machines do
not.

**Trade-off.** You must *design* the states up-front. Yuneta does not
let you "just call a method" on another gobj — there are no methods.

### 2. One thread per yuno; scale horizontally across cores

A yuno is a single-threaded process. There is no thread pool, no lock,
no mutex, no atomic counter inside the framework. Scaling happens by
running **one yuno per CPU core** and exchanging events between them
over the inter-event protocol.

**Why.** Shared-memory concurrency is where most bugs in long-running
C services live. Removing it removes a whole category of problems:
data races, deadlocks, priority inversion, cache-line ping-pong, and
the cost of reasoning about lock order. It also makes profiling and
tracing trivial — each yuno has one call stack at any instant.

**Trade-off.** A single CPU-bound action blocks the whole yuno until
it returns. The framework assumes every action is non-blocking.

### 3. `io_uring` instead of `epoll`

The asynchronous engine (`yev_loop`) is built on Linux **`io_uring`**.
Every file-descriptor operation — accept, connect, read, write, timer,
signal — is submitted through the same ring and its completion drives
a callback. Filesystem events go through `fs_watcher` (an inotify
wrapper in `timeranger2`) whose inotify fd is itself read through
`io_uring`, so completions still land on the same loop.

**Why.** `io_uring` unifies the API (one ring for I/O, timers,
signals, and — via the inotify fd — fs events) and cuts the syscall
cost per operation. It also supports linked and batched submissions,
which matters for the per-yuno message throughput Yuneta targets.

**Trade-off.** Requires a reasonably recent Linux kernel. Not portable
to non-Linux systems; the ESP32 port (`kernel/c/root-esp32`) lives
outside `yev_loop` and relies on the ESP-IDF runtime instead.

### 4. JSON is the *only* in-flight data format

Every event payload, every persistent record, every log entry, every
command, every stats response is a `json_t` from Jansson. There is no
"struct for speed, JSON for the wire" split.

**Why.** A single format means: one serializer, one schema tool, one
debug printer, one diff, one test assertion, one persistence layer.
It also means C and JavaScript implementations can exchange messages
byte-for-byte without any generated code. The JS frontend talks to the
C backend through the same `kw` payloads that two C gobjs exchange in
the same process.

**Trade-off.** JSON parsing is not free. Yuneta pays the cost on
purpose; its throughput targets are in the "many thousands of
messages per second per core", not in the "tens of millions".

### 5. Function-oriented classes, language-portable by construction

A GClass is a **plain descriptor**: a name, a state table, an event
table, an attribute schema (SData), a list of commands, and a set of
action callbacks. It is **not** a C struct with methods and it is
**not** a subclass of anything. There is no inheritance, no virtual
table, no method overriding.

**Why.** A GClass is data, not a language construct. The same GClass
layout exists in C (`kernel/c/gobj-c`) and in JavaScript
(`kernel/js/gobj-js`), with the same public API, so a JavaScript
frontend can drive a C backend as if they were two halves of the same
program. The SData schema carries type, default, persistence, stats,
and authorization flags in one declaration, consumed identically by
both languages.

**Trade-off.** Developers used to classical OO need to unlearn
inheritance. Composition happens through the "bottom gobj" mechanism
and through event routing, not through class hierarchies.

### 6. Everything lives in a hierarchical tree — the yuno

Every gobj has exactly one parent. The root of the tree is the
`__yuno__` itself. A deployable process is precisely this tree plus
the runtime that drives it.

**Why.** A tree gives you lifetimes, authorization scopes, and
navigation for free. Starting a branch starts every gobj in it;
destroying a parent destroys the children; a command from the control
plane can be routed by path; traces and logs always know the full
ancestor chain of the gobj that produced them.

**Trade-off.** Peer-to-peer relationships between sibling gobjs go
through publish/subscribe and events, not through direct pointers.
This is by design (see decision 1) but takes getting used to.

### 7. Persistence is append-only time-series, with a graph on top

`timeranger2` is the one persistence primitive. It writes append-only
records per topic, per key, and indexes them by monotonic
`g_rowid` / `i_rowid` and by time. Everything else — the `tr_msg2db`
dict store, the `tr_queue` durable queue, the `tr_treedb` in-memory
graph — is built on top of the same log.

**Why.** Append-only logs are the simplest data structure that gives
you crash safety, audit trails, replay, and zero-copy reads. A graph
database (TreeDB) emerges naturally when you index the log by
hook/fkey relationships in memory and write only the children on
link / unlink — which is exactly what TreeDB does. One primitive,
several views.

**Trade-off.** You need disk space and you need to think about
compaction. Yuneta exposes compaction tools but does not hide the
mechanics.

### 8. The control plane is first-class, not bolted on

Every running yuno exposes **commands** and **stats**. Operators reach
them through a local socket via the `ycommand` CLI; other yunos reach
them by sending the same commands as events over the inter-yuno
protocol. In both cases the surface is identical. Commands are
declared on the GClass with name, parameters, authorization, and help
text — the same SData mechanism as attributes.

**Why.** Operable long-running services need to be inspected,
reconfigured, and driven without restarts. Making commands a property
of the GClass means every gobj written for Yuneta is operable by
construction — no separate "management interface" to design.

**Trade-off.** Every public command is part of the API. You have to
version and authorize it like any other contract.

## What these decisions buy you

- **Predictable long runs.** No lock contention, no GC pauses, no
  thread-explosion, no hidden pools.
- **Replayability.** Every state change is an event; every event can
  be logged; persistent state is an append-only log. Incidents can be
  reproduced offline.
- **Operable from day one.** Commands and stats come for free with
  every GClass; `ycommand` works on anything.
- **C / JavaScript interoperability.** The same SData schema and the
  same `kw` payloads mean a browser can act as if it were a local
  gobj in a C yuno.
- **Horizontal scaling by CPU.** Add a yuno per core; communicate via
  events. There is nothing else to tune.

## What they cost you

- **An up-front design step.** You must decide the states, events, and
  attributes of each GClass before writing the action callbacks.
- **Learning a different OO model.** No inheritance, no methods —
  composition through "bottom gobj" and through events.
- **A non-zero JSON cost** on every message in flight.
- **Linux-first.** Windows and macOS are not targets; the ESP32 port
  lives outside `yev_loop` and uses the ESP-IDF runtime instead.

## Where to go next

If the trade-offs above match what you need, the recommended path is:

1. [Domain Model](domain_model.md) — learn the vocabulary these
   principles operate on (realms, entities, messages, CRUDLU).
2. [Installation](../installation.md) — get a working build environment.
3. [Basic Concepts](../guide/basic_concepts.md) — concrete mechanics
   of [GClass](../guide/basic_concepts.md#basic_gclass),
   [gobj](../guide/basic_concepts.md#basic_gobj), and
   [yuno](../guide/basic_concepts.md#yuno).
4. [GClass Guide](../guide/guide_gclass.md) — build your first GClass.
5. The **API reference** sections in the sidebar.

Optional:

- [Inspiration](philosophy.md) — the humanist angle that shaped the
  framework's vocabulary.
