# The Typed-Graph Model

This page answers the question **"what *is* Yuneta, as a model?"** —
the conceptual claim that underpins the engineering choices laid out
in [Design Principles](design_principles.md) and the vocabulary in
[Domain Model](domain_model.md).

The short version: **Yuneta treats data and behavior as two views of
the same typed graph**. Most stacks have a database for "what is" and
a separate process model for "what happens"; Yuneta unifies them under
one set of primitives. Everything you build — a node in a topic, a
running service, a TCP socket, a subscription, an authorization rule —
is an instance of a typed class connected to other instances by typed
links.

This page is for readers who want to understand the framework as a
whole before learning its surface area. None of it is required to
write working code, but it explains why the surface area is shaped the
way it is.

## The unit is not "node + edge"; it is "typed instance with typed bindings"

A graph in mathematics has nodes and edges. The distinctive choice in
Yuneta is that **every edge carries a contract**:

- A treedb `hook` is not "there is a link" — it is "this kind of link,
  between *these* two topics, with *this* name, with *this*
  cardinality, validated at insert time".
- A gobj event like `EV_ON_OPEN` is not a string — it is a declared
  event type in the `event_types[]` table with a schema for its `kw`
  payload, and the FSM rejects events the current state does not
  accept.
- A command is not a free-form RPC — it is a `SDATACM2` registration
  with a `pm_xxx[]` parameter descriptor, type-checked before reaching
  the handler.
- A subscription is not "callback registered" — it is a typed binding
  between publisher gobj, event type, and subscriber gobj that the
  framework can introspect at runtime.

This is what separates the model from neighbouring designs:

- **Untyped graph databases** (RDF, basic property graphs) type the
  nodes but treat the edge as opaque metadata.
- **Untyped actor systems** (Erlang/OTP, classic Akka) type the
  process but pass arbitrary terms as messages.
- **Object-oriented frameworks** type the object but rarely enforce a
  declared FSM per class.

Yuneta types all three at once: the node, the link, and the lifecycle.

```{figure} ../_static/treedb_graph.svg
:alt: The departments topic hooks the users topic; each user node carries a department fkey pointing back. Port colour marks the linked topic.
:width: 100%

A treedb graph reads left to right. The `departments` topic declares a
`users` **hook**; each `users` node carries a `department` **fkey** back.
The binding is typed — *this* link, between *these* two topics, with
*this* name. A port's colour marks the topic it links to. Link/Unlink
write the **child's** fkey, so only the child is persisted; the parent's
hook list is rebuilt in memory on load.
```

## Two graphs, one set of primitives

Yuneta has two graphs running side by side. They are not analogous —
they are isomorphic in primitive shape:

| Information plane (treedb) | Behavior plane (gobj-tree)            |
|----------------------------|---------------------------------------|
| `topic`                    | `gclass`                              |
| `node` (with `id`)         | `gobj` (with [`gobj_name`](#gobj_name)) |
| `hook` / `fkey`            | `parent` / `subscribe` / `bottom_gobj`|
| field schema (`sdata_desc_t`) | attribute schema (`sdata_desc_t`) — *same macro family* |
| `EV_TREEDB_UPDATE_NODE`    | `EV_ON_OPEN`, `EV_TIMEOUT`, …         |
| `timeranger2` (persistence)| `yev_loop` (scheduler)                |

The same `SDATA()` macro family declares the columns of a user record
in `treedb_authzs` and the attributes of a [`C_TCP`](#gclass-c-tcp) gobj. The same
notion of "subscriber" expresses both "this gobj listens for events
from that one" and "this node points to that one". One mental model
covers both planes — the difference is whether the binding is *static*
(data: who relates to whom) or *dynamic* (behavior: who reacts to
whom).

See [GObj crash course](../../../yunos/c/yuno_agent/GOBJ.md) and
[TreeDB crash course](../../../yunos/c/yuno_agent/YUNO_TREEDB.md) for
the concrete APIs on each plane.

## What this lets you model

Because both planes share primitives, you can express almost any kind
of organisation as some combination of typed nodes and typed links:

- **Strict hierarchies** — a single parent hook. The gobj-tree itself
  is built this way; so is any topic that links back to a parent of
  the same type.
- **Matrix or many-to-many** — multiple `fkey` fields on a child node,
  each pointing to a different parent topic. Bipartite (users ↔ roles)
  or multipartite (users ↔ roles ↔ permissions ↔ services) just fall
  out of the schema. `treedb_authzs` is exactly this shape.
- **Workflows and state machines** — the topic-of-states pattern: a
  topic where each row is a state and `next_state` is a self-fkey.
  And, on the behavior plane, every gclass already *is* a finite state
  machine, so workflows expressed as gclasses get FSM enforcement for
  free.
- **Communication topologies** — gates, channels, protocols and
  subscriptions form a graph parallel to the data graph. Who can send
  what to whom, with delivery semantics (sync command vs. published
  event vs. queued message) baked into the binding type.
- **Versioned relations over time** — every treedb write appends a new
  record with a fresh `g_rowid` and a timestamp; the current view is a
  projection of the latest non-deleted row per `id`. Historical
  reconstruction is a matter of choosing a different cut-off.

```{figure} ../_static/node_history.svg
:alt: A node id accumulates records by rowid (create, update, link, unlink, update); the current node is a projection of the highest rowid, and a link or unlink is itself a record in the history.
:width: 100%

A node is not a cell — it is its append history, `node`&#8319;. Each
**create / update / link / unlink** appends a row; a relationship change
is an *event in the history* (it saves the child), not an in-place edit.
The current node is the projection of the highest `g_rowid`. This is why
a durable delete must tombstone **every** row of the id, and why unlink
must find the right version — the two fixes shipped in 7.5.1.
```

## What this *doesn't* let you model cleanly

The same constraint that gives you uniform composition also has costs.
Worth being honest about where the model strains:

- **Schemaless, rapidly-iterating data** — you pay the cost of
  declaring topics, fields and hooks upfront. Tools that let you stuff
  arbitrary JSON and worry later (MongoDB, Firestore) get you faster
  to a first running prototype.
- **OLAP / analytical workloads over large relations** —
  `timeranger2` is an append-only OLTP store optimised for primary-key
  and time-range access. Aggregate joins across tens of millions of
  rows live elsewhere (Postgres, ClickHouse), with Yuneta acting as
  the upstream that *produces* those rows.
- **Eventually-consistent distributed state** — Yuneta is one yuno per
  CPU core, and multi-yuno coordination happens through explicit
  message gates. The model is closer to CSP than to Paxos. If you
  need transparent multi-master replication, that is a layer you
  build *on top of* Yuneta, not something the framework gives you.
- **Truly opaque payloads** — anything that genuinely cannot be typed
  ends up as a `json_t *` blob or a base64 string in a `user_data`
  field. Those are the cracks where the model lets through what it
  could not classify, and they are worth treating as design smells
  when they appear in new code.

## The implicit axiom

Pulled tight, the philosophy is one sentence:

> A system is fully described by **(a)** the set of typed instances
> that exist plus **(b)** the set of typed bindings between them.
> Everything else — state, capacity, behavior, authorization,
> communication — is an attribute of an instance or a property of a
> binding.

This is reductive in the same way physics is reductive. It forces you
to express things in a small vocabulary, and in exchange the rest of
the system can reason about them uniformly. The reward shows up at
scale: tools, traces, FSM enforcement, supervision and substitution
all keep working as the codebase grows, because every new piece slots
into the same shape.

The price shows up when something *really* does not fit (a free-form
blob, an unowned side-effect, an operation with no clear locus). You
either deform it to fit or leave it as a leak in the model. Catching
those moments early — and either tightening the schema or admitting
the leak explicitly — is the running design discipline.

## What you get in practice

The payoffs that justify the upfront typing cost:

- **Uniform composition at every scale.** A TCP socket is a gobj; a
  protocol on top is a gobj that owns the socket as `bottom_gobj`; a
  service is a gobj that owns the protocol; a yuno is a gobj that
  owns services; the agent is a gobj that owns yunos. The same
  primitive ([`gobj_subscribe_event`](#gobj_subscribe_event),
  [`gobj_send_event`](#gobj_send_event)) works at every level.
- **Substitution by contract.** Swap OpenSSL ↔ mbedTLS under `C_YTLS`
  and the layers above do not notice. Swap `c_tcp` for
  `c_unix_socket` and the protocols stay blind. The contract is the
  gclass signature; the implementation behind it is interchangeable.
- **Structural traceability.** [`gobj_short_name(g)`](#gobj_short_name) returns the full
  path through the tree
  (`C_YUNO^...^C_AUTH_BFF^auth_bff^C_HTTPS_CL^...^C_TCP^...`). You
  know *where* in the graph something is happening without bolting on
  ad-hoc tracing. The same applies to nodes — every treedb record
  carries metadata identifying its topic, treedb and rowid.
- **One vocabulary to learn.** Once you understand "typed class +
  typed instance + typed binding + schema", you can read any layer of
  Yuneta — kernel, ytls, treedb, application service, GUI declarative
  shell — without switching mental models.

## The empirical justification

The implicit bet of Yuneta is that **paying to type everything
amortises over many years of production**. The bet has been honoured:
the same model (under earlier names — v2, v6) has been running at
industrial scale for over fifteen years in deployments where downtime
is expensive and operator turnover is real. The current v7 codebase
inherits from that empirical history; consequently, when a design
choice looks strange, the answer is more often "this was learned in
production" than "this is theory".

## Where to go next

- [Design Principles](design_principles.md) — the engineering "why"
  for each individual decision.
- [Domain Model](domain_model.md) — the concrete vocabulary you use
  to model reality with the primitives described here.
- [Inspiration](philosophy.md) — where the words came from, written
  in a non-technical register.
- [GObj framework crash course](../../../yunos/c/yuno_agent/GOBJ.md) — concrete
  API on the behavior plane.
- [timeranger2 + treedb crash course](../../../yunos/c/yuno_agent/YUNO_TREEDB.md) —
  concrete API on the information plane.
