# Domain Model

This page describes the vocabulary Yuneta uses to model reality:
realms, entities, relationships, messages, and the CRUDLU operations
on them. It is the concrete counterpart to the
[Design Principles](design_principles.md) (*why* the framework is
built this way) and to the [Inspiration](philosophy.md) page (*where
the words come from*).

In Yuneta we seek models (patterns) that allow us to represent and visualize all kinds of realities.

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

The basic operations on **entities** are the classic CRUD:

- **C**reate
- **R**ead
- **U**pdate
- **D**elete

But when we add **relationships** between entities, we need two additional operations:

- **L**ink
- **U**nlink

**CRUDLU** = **Create**, **Read**, **Update**, **Delete**, **Link**, **Unlink**
