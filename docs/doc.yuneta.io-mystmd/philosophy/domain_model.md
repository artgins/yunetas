# Domain Model

This page describes the vocabulary Yuneta uses to model reality:
realms, entities, relationships, messages, and the CRUDLU operations
on them. It is the concrete counterpart to the
[Design Principles](design_principles.md) (*why* the framework is
built this way) and to the [Inspiration](philosophy.md) page (*where
the words come from*).

In Yuneta we seek models (patterns) that allow us to represent and
visualize all kinds of realities.

:::{note} How this vocabulary maps onto the code
The abstract terms below have direct equivalents in the framework:

| Concept here | In the framework |
|---|---|
| **Role** (class) | A [GClass](../guide/basic_concepts.md#basic_gclass) registered at startup |
| **Instance** | A [gobj](../guide/basic_concepts.md#basic_gobj) created by `gobj_create(name, gclass, kw, parent)` |
| **Realm** | A running [yuno](../guide/basic_concepts.md#yuno) or a logically-grouped set of them |
| **Parent / Child** | A gobj and the gobjs it created (`parent` argument of `gobj_create`) |
| **Service / Client** | A gobj registered with `gobj_create_service()` and the gobjs that address it by service name |
| **Message** | An event name + a `kw` JSON payload sent with `gobj_send_event()` |
| **Persisted record** | A row in a `timeranger2` topic, or a node in a `tr_treedb` graph |

Read the next sections as *the words we use*; the reference to *what
the code actually calls them* is always in this table.
:::

---

## Realms

Yuneta systems are organized into **realms**.

Each realm has three properties:

| Property | Description |
|----------|-------------|
| **Role** | Class of the realm |
| **Name** | Instance of the realm |
| **Owner** | Owner of the realm |

### Realm Terms

- The **Role** defines the class of the realm.
- The **Name** is an instance of that class.
- Each realm instance has an **Owner** who controls it.

The frontend URL defines which realm the user wants to enter.
The owner of a realm can add other users and define their authorization levels.

---

## Entity/Relationship Model

We describe reality by classifying it into **Entities** and the **Relationships** between them.

```
    ┌────────────┐         ┌────────────┐
    │  Entity A  │─────────│  Entity B  │
    └────────────┘         └────────────┘
         │          relationship
         │
    ┌────────────┐
    │  Entity C  │
    └────────────┘
```

---

## Entities

We describe **entities** using the pair model:

| Model | Also known as |
|-------|---------------|
| **Role** / **Instance** | **Class** / **Object** |
| | **Table** / **Record** |
| | **Group** / **Element** |

```
    ┌──────────────────┐
    │    Role (Class)   │
    ├──────────────────┤
    │  instance 1      │
    │  instance 2      │
    │  instance 3      │
    │  ...             │
    └──────────────────┘
```

---

## Relationships

We describe **relationships** also using pair models:

- **Parent** / **Child** — The parent (or a factory) creates and connects to the child.
- **Service** / **Client** — The client knows and connects to the service.

```
    Parent ──creates──► Child

    Client ──connects──► Service
```

---

## Message Transport

**Entities** exchange **messages** through their **relationships** (links).

Behavior patterns:

- Parent sends events/commands **down** to children.
- Children send events/responses **up** to parents.
- Services publish events to subscribed clients.
- Clients send requests to services.

---

## Message Content

The data model used in message content — both internally and externally, for persistence and transport — is **key/value**.

```
    ┌──────────┬──────────┐
    │   Key    │  Value   │
    ├──────────┼──────────┤
    │ name     │ "Alice"  │
    │ age      │ 30       │
    │ enabled  │ true     │
    └──────────┴──────────┘
```

Yuneta can also add the **time-series** model to transported messages:

```
    ┌──────────┬──────────┬─────────────────────┐
    │   Key    │  Value   │     Timestamp        │
    ├──────────┼──────────┼─────────────────────┤
    │ temp     │ 22.5     │ 2024-01-15 10:00:00 │
    │ temp     │ 23.1     │ 2024-01-15 10:01:00 │
    │ temp     │ 22.8     │ 2024-01-15 10:02:00 │
    └──────────┴──────────┴─────────────────────┘
```

---

## Encoding and Persistence

All models are represented in **JSON**, both in memory and on disk, using:

- **Hierarchical** and **Graph** databases
- **Key/Value** storage
- **Time-Series** storage

Example topologies:

```
    Tree (hierarchical):          Graph:

         A                      A ─── B
        / \                     │ \   │
       B   C                    │  \  │
      / \                       C ── D
     D   E


    Key/Value:                  Time-Series:

    ┌─────┬───────┐            ──●──●──●──●──●──► t
    │ k1  │ v1    │              v1  v2  v3  v4
    │ k2  │ v2    │
    └─────┴───────┘
```

---

## CRUDLU

Most systems describe entities with the classic **CRUD** operations:

- **C**reate
- **R**ead
- **U**pdate
- **D**elete

Yuneta adds two more because the [TreeDB](../guide/guide_timeranger2.md)
graph store makes relationships first-class citizens, not foreign keys
managed by convention:

- **L**ink — attach an existing child to an existing parent via a
  declared hook. The operation is persisted by writing the child's
  `fkey` field (see [TreeDB in CLAUDE.md](../guide/guide_timeranger2.md)).
- **U**nlink — the inverse: clear the child's `fkey`, persist the
  change.

**CRUDLU** = **C**reate, **R**ead, **U**pdate, **D**elete, **L**ink,
**U**nlink. Every TreeDB API call is one of these six shapes.

## Where to go next

- [Design Principles](design_principles.md) — the engineering decisions
  these concepts sit on (in particular, decision 7 on append-only
  persistence and decision 8 on the control plane).
- [Basic Concepts](../guide/basic_concepts.md) — the same concepts
  from the implementation side.
- [Timeranger2 Guide](../guide/guide_timeranger2.md) — how CRUDLU
  actually works on disk and in memory.
