# Architecture

This page answers the question **"how do you assemble a Yuneta system above
one gobj?"**. [Design Principles](design_principles.md) gives the reason for
each separate decision. [The Typed-Graph Model](typed_graph_model.md) gives the
model that holds the decisions together. This page describes the result: three
layers, one message mechanism, and one word — **role** — that keeps the same
meaning in all three.

```{figure} ../_static/architecture_layers.svg
:alt: Three layers of a Yuneta system. At the bottom the compiled behavior layer, where the gobjs of a yuno exchange events addressed by pointer and two yunos exchange ievents addressed by service name. In the middle the persisted information layer, treedb topics linked by hooks and fkeys, written with CRUDLU and answering with EV_TREEDB_NODE events. At the top the dynamic layer of services and roles - realm, service, role, user - which is the same set of linked nodes read as the structure the end user sees.
:width: 100%

Three layers, and one message mechanism through all of them. The **behavior**
layer is compiled: a gobj addresses another gobj by pointer, and a yuno
addresses another yuno by service name. The **information** layer is persisted
and changes while the yuno runs. The **services and roles** layer is not a
third store — it is the same linked nodes, read as the structure the end user
works with.
```

## A class is a role

A gclass header exposes two things: `GOBJ_DECLARE_GCLASS(C_FOO)` and
`register_c_foo()`. It publishes no structure. The private data of an instance
stays behind an opaque pointer. Every interaction goes through the five public
mechanisms: attributes, commands, events, local methods and statistics.

A class with no visible structure keeps one identity only: what it does for the
other objects. That identity is a **role**.

Most object systems cannot make this claim. A C++ or a Java class is also a
data type, so part of its identity is its structure. Yuneta hides the structure
on purpose, and the class becomes the role.

The framework already uses the word one level above. A yuno has a **role** and
a **name**, and the documentation writes the pair as `role^name`. The role is
the class of the process. The name is the instance. A gclass and a
[`gobj_name`](#gobj_name) are the same pair one level lower.

### The rule

> **Name a gclass for the role it plays.**

Apply the rule in reverse to test a design. When you cannot name a gclass by
its role, the gclass has no role. It is a bag of code inside a gobj wrapper, or
it is two roles in one class. This is the test the coding rules already apply
to functions: a function whose honest name tells the reader nothing is a
function to delete, and not a function to rename. The suffixes `-Manager`,
`-Helper` and `-Handler` are the classic symptom, because they name the absence
of a role.

Examples from the current tree:

| GClass | What the name does |
|---|---|
| `C_PROT_HTTP_CL`, `C_PROT_HTTP_SR`, `C_TCP_S`, `C_PROT_MODBUS_M` | The suffix carries the role. A protocol name alone does not say client, server or master. |
| `C_IDP_KEYCLOAK` | Role first, implementation second. This is the correct form of the preference for the domain word over the vendor name. The vendor name qualifies a role, and never replaces one. Compare `C_POSTGRES`, where the vendor name stands alone and the role does not appear. |
| `C_TIMER0`, `C_RESOURCE2` | The `0` and the `2` name an implementation and a version. The reader learns nothing about the part the gobj plays, and `C_TIMER0` is a repeated source of mistakes for that reason. |

### Two levels of role

The rule has a limit, and the limit is useful.

- A **gclass** names the *generic* role: the part the class knows how to play
  anywhere.
- A [`gobj_name`](#gobj_name) names the *situational* role: the part this
  instance plays in this tree.

[`C_TCP`](#gclass-c-tcp) does not say what this socket does here. The instance
name says it. Push the situational role into the class name and you get one
gclass for each place it is used. Keep the two levels apart and one gclass
serves every place.

## One mechanism, two scopes

Yuneta has one way for objects to talk, and it is the message. What changes
when a message leaves the process is not the mechanism. It is the address.

- **Inside a yuno.** A gobj sends with
  [`gobj_send_event()`](#gobj_send_event) or publishes with
  [`gobj_publish_event()`](#gobj_publish_event). The destination is a pointer,
  and the delivery is synchronous.
- **Between yunos.** The same events travel as **ievents** through
  [`C_IEVENT_CLI`](#gclass-c-ievent-cli) and
  [`C_IEVENT_SRV`](#gclass-c-ievent-srv). The destination is a **name**.
  `c_ievent_srv.c` resolves it with `gobj_find_service()` and keeps the
  `src_service` of the sender for the answer.

One rule follows from this: **only a named service can be the source or the
destination of an inter-yuno message.** A pointer does not travel over a
websocket. A name does. A routed view, a pure child, or any unnamed gobj can
never hold one end of an inter-yuno conversation. The log message
*"event ignored, service not found"* (from `c_ievent_srv.c`, and in the JS runtime
*"gobj service not found"*) reports a violation of this rule. Correct
the sender.

| Scope | Structure | Message | Address |
|---|---|---|---|
| Inside a yuno | a tree of gobjs | event | pointer |
| Between yunos | a graph of treedb nodes | ievent | service name |

The two rows repeat one shape at two scales. That repetition is deliberate. It
is the fractal consistency the [Inspiration](philosophy.md) page describes in a
non-technical register.

## TreeDB is the structure, not the channel

A frequent first reading of the table above is that treedb is how yunos talk at
the upper level. It is not. TreeDB holds the **structure**: what exists, how it
relates to the rest, and what configuration it carries. Messages remain the
only channel at every scale.

The agent is the clearest example. Its yunos, binaries, configurations and
realms are nodes of a treedb. The topology of a node **is** that graph.

Two consequences matter in practice.

**The store is a meeting point.** When two yunos cannot address each other
directly, the shared treedb is where they meet. This is a deliberate pattern,
and not a workaround.

**The store sends messages of its own.** A treedb publishes
`EV_TREEDB_NODE_CREATED`, `EV_TREEDB_NODE_UPDATED`, `EV_TREEDB_NODE_DELETED`,
and `EV_TREEDB_NODE_LINKED` and `EV_TREEDB_NODE_UNLINKED` (unless it is
opened with `with_link_events` off, or switched off with `set-link-events`). A change of shared state
returns through the one channel that exists. This is also why Yuneta refuses
polling. The producer publishes, and the consumer subscribes.

**The store scales by the key.** A treedb is held in memory, so the RAM of
one node limits it. It grows by adding nodes, not by growing one node. Every
key has its own directory, and an entry gateway
([`C_MQIOGATE`](#gclass-c-mqiogate)) routes each message to the node that
holds its key. A feature that needs every key of a topic in one process is a
convenience of one node, not part of the model. Examples are a multi-key
iterator of `C_TRANGER` (`rkey`) and the mark that only the process that
deleted a key sees. See [the key](#philosophy-the-key) on the Inspiration
page.

## Under the information layer: timeranger2

A treedb holds its graph in memory, and it persists every change in
[timeranger2](#tranger2_startup). timeranger2 knows nothing about graphs. It is
an append-only log of JSON records, and the information layer stands on it.
Four properties of the log explain what you see higher up.

**Append-only.** A record is never edited. An update of a treedb node appends
a new record of that node, and the record before it stays in the file. This is
why `g_rowid` only grows, and why a snap can return a topic to an earlier
state.

**By topic, then by key.** A topic is a directory. Each key (the primary key of
a record) has its own directory under it. This is the "every key has its own
directory" of the section above: the key is the unit a store grows by.

**Cut into files by time.** The records of a key go into files named by
`filename_mask`, a `strftime()` format applied to the time `t` at which the
record was saved. The default is `%Y-%m-%d`, one file per day. A small store
takes a coarser mask. Retention deletes the old files of a key, whole.

**Two times.** `t` says when the record was saved. `tm` says when the event it
carries happened, and the producer writes it. The files are cut by `t` only.

The layout on disk, with the default mask:

```
<database>/
  __timeranger2__.json      ← the metadata of the database; the master opens it to write
  <topic>/
    topic_desc.json         ← the fixed metadata: pkey, tkey, filename_mask
    topic_cols.json         ← the fields of the records (optional)
    topic_var.json          ← the metadata that changes: topic_version, last_rowid_id
    delete_seq.json         ← the order of the key deletes, for the readers
    disks/                  ← a reader that follows the topic has a directory here
    keys/
      <key>/
        2026-11-18.json     ← the records, appended
        2026-11-18.md2      ← 32 bytes per record: t, tm, offset, size
        2026-11-19.json
        2026-11-19.md2
```

The master writes `delete_seq.json` when it opens the topic, with the value 0.
A `<file>.unordered` file beside a `.md2` file marks that a record went back in
`t`, so a load reads that file whole.

The agent opens its treedb with a coarser mask, because its store changes
rarely (`yunos/c/yuno_agent/src/agent_treedb.c`):

```c
json_t *kw_treedb = json_pack("{s:s, s:i, s:s, s:o, s:b}",
    "filename_mask", "%Y",          // one file per year
    "exit_on_error", 0,
    "treedb_name", treedb_name,
    "treedb_schema", jn_treedb_schema_yuneta_agent,
    "impose_c_schema", 1
);
```

treedb is not the only store on the log. msg2db (one value per key) and its
queue (used by the MQTT broker) are built on it too, and a yuno that wants raw
time series uses it directly through [`C_TRANGER`](#gclass-c-tranger). The full
API and its traps are in the
[timeranger2 + treedb crash course](../../../yunos/c/yuno_agent/YUNO_TREEDB.md).

## The dynamic layer: services and roles

Everything above this section is compiled. A gclass registers at start-up. The
shape of the gobj tree lives in `main.c`. You cannot add a role to that tree
while the process runs.

On the linked topics of a treedb, Yuneta builds a **second structure of
services and roles**, and that structure is **data**. It is nodes and links.
You change it with [CRUDLU](domain_model.md#crudlu) while the system runs. No
rebuild. No restart.

**This third layer is not a third store.** It is a reading of the second one.
The same linked nodes that an operator reads as topics and hooks, an end user
reads as the services available and the role held over them. An end user never
sees a gclass.

### Why the two layers stay apart

- The **compiled** layer states what the software **can** do. That is a
  capability, and it is identical in every installation of the same binary.
- The **dynamic** layer states what **this** installation does, and **for
  whom**. That is deployment and authorization, and it differs for each
  customer, each realm and each day.

Merge the two and every new customer needs a new build. Keep them apart and one
binary serves every installation, because the part that varies is data.

Three examples already in the tree:

- **Realms.** A realm carries a Role, a Name and an Owner. The owner adds users
  to the realm and sets the authorization level of each one. See
  [Domain Model](domain_model.md#realms).
- **`treedb_authzs`.** Users, roles, permissions and services are four topics
  linked to each other. An authorization answer is a walk over that graph, and
  not a lookup in a compiled table.
- **The agent.** Yunos, binaries and configurations are nodes. A deploy writes
  to the graph, and does not change the agent binary.

### The bridge between the layers

A node in the dynamic layer names a role. The compiled layer implements that
role. The rule at the top of this page is what lets the two meet: **role** keeps
one meaning on both sides, so a name that an operator writes into a treedb node
resolves to a gclass that a developer registered in C.

### The discipline the dynamic layer needs

A declarative JSON structure is sugar over a runtime API, and that API must
stay available while the yuno runs. When a feature must change a structure that
the configuration describes, and no API exists for the change, write the API.
Do not work around the static configuration.

A layer you can change only at start-up is a build step with a different file
format. It gives the end user nothing.

## Infrastructure: the three layers, run by the operator

Versioning binaries and configurations, deploying them, and managing a network
of nodes is not a fourth layer. It is an application built with the three
layers, and its user is the operator.

- **The agent is a yuno.** Its main service is `C_AGENT`, a gclass on gobj-c
  like any other. Every node runs two agents, `yuneta_agent` and
  `yuneta_agent22`, so that each one can upgrade the other.
- **Its state is a treedb**, `treedb_yuneta_agent`. Its topics are `realms`,
  `yunos`, `binaries`, `configurations` and `public_services`. A yuno node
  links to the binary and the configuration it runs.
- **A version is a node.** In `binaries` and `configurations` the version is a
  secondary key (`pkey2`), and in `yunos` the release is. A new version is a
  new node beside the old one. The old node stays, and so does its binary
  file. Versioning needs no store of its own: it is rows of the graph.
- **A rollback point is a snap.** `shoot-snap` tags the current state of the
  graph. `activate-snap` returns every topic to that tag, and restarts the
  yunos whose launch changed. This works because the log under the graph is
  append-only (see the timeranger2 section above).
- **The fleet is messages.** The agent of each node opens a connection to the
  control center (`controlcenter`, a yuno too), and an administrator commands
  any node through it. The `yunetas` CLI sends the same commands from a
  workstation (`sync-binaries`, `sync-configs`, `upgrade-yunos`).

A version upgrade on one node, and its rollback:

```bash
ycommand -c 'install-binary id=<role> content64=$$(<role>)'  # a new binaries node; the old one stays
ycommand -c 'shoot-snap name=pre-upgrade'                    # a tag in the graph: the rollback point
ycommand -c 'find-new-yunos create=1'                        # a new yunos node at the new release
ycommand -c 'deactivate-snap'                                # reload: the highest release runs

ycommand -c 'activate-snap name=pre-upgrade'                 # if it misbehaves: the graph as it was
```

The same agent, on a node of the fleet, through the control center (`<cc>` is
the id of the controlcenter yuno):

```bash
ycommand -w 10 -c 'command-yuno id=<cc> service=controlcenter command=command-agent agent_id=<host> cmd2agent="list-yunos"'
```

To leave a snap after a rollback, follow [the rollback
recipe](#yuno-lifecycle-rollback): a plain `deactivate-snap` runs the highest
release again. The step-by-step guide is [Deploying yunos](../deploying-yunos.md).

## The three layers

| Layer | Made of | Changed by | Read by |
|---|---|---|---|
| **Behavior** | gclasses, gobjs, state machines | a rebuild | the developer |
| **Information** | topics, nodes, hooks, fkeys | CRUDLU, while running | the operator |
| **Services and roles** | the same linked nodes, read as a structure | CRUDLU, while running | the end user |

One mechanism crosses all three, and it is the message. One word crosses all
three, and it is **role**. The first keeps the system observable at every
scale. The second keeps the vocabulary of the end user and the vocabulary of
the C developer in agreement.

:::{note} The same three layers, running
[**The three layers, and what a change costs**](https://doc.yuneta.io/three-layers)
is this page as a walkthrough. Add a service, hand a user a role, and watch the
rebuild counter stay at zero. The switch that matters is *read as*: it renders
the same node array as topics and hooks, and then as services and roles, which
is the claim of the third layer in one click.
:::

## Where to go next

- [The three layers, running](https://doc.yuneta.io/three-layers) — this page
  as a walkthrough you step through.
- [The Typed-Graph Model](typed_graph_model.md) — the two planes of the model
  this page builds a third layer on.
- [Domain Model](domain_model.md) — realms, entities, relationships and
  CRUDLU, the vocabulary of the dynamic layer.
- [Design Principles](design_principles.md) — the engineering reason for each
  decision named here.
- [Inter-process communication](../../../yunos/c/yuno_agent/IPC.md) — the
  concrete gates, ievents and message flow.
- [timeranger2 + treedb crash course](../../../yunos/c/yuno_agent/YUNO_TREEDB.md) —
  the concrete API of the information layer.
