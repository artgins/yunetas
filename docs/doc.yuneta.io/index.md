---
title: Yuneta Simplified
---

# Yuneta Simplified

::::{grid} 1 1 2 2

:::{grid-item}

## *An Asynchronous Development Framework*

**Current version: [7.12.0](https://github.com/artgins/yunetas/tree/7.12.0)**

*Documentation updated: 2026-08-10*
:::

:::{grid-item}

```{image} ./_static/yuneta-image.svg
:width: 150px
:align: center
```
:::
::::

`Yuneta Simplified` is a **development framework** focused on **messaging** and
**services**, built on the
[event-driven](https://en.wikipedia.org/wiki/Event-driven_programming),
[automata-based](https://en.wikipedia.org/wiki/Automata-based_programming) and
[object-oriented](https://en.wikipedia.org/wiki/Object-oriented_programming)
paradigms. Every component is a state machine, every message is JSON, and the
state machine is where things happen — so the trace of a running system *is*
its execution log.

It is a **real-time system** for [Linux](https://en.wikipedia.org/wiki/Linux),
with development, testing and deployment included. It collects data from
devices, adapts protocols between systems, and publishes, subscribes and
queries **messages** in real time, with **historical** storage on
**time-series**, **key-value**, **flat-file** and **graph** stores. Messages
travel encrypted or in plain text, and live on disk or only in transit.

Yuneta produces **fully static binaries** by default — no shared libraries, no
dynamic linker. A compiled yuno is copied to any Linux machine of the same CPU
architecture and runs there, with nothing to install. There are versions in
**C** (the reference implementation) and **JavaScript** (browser and Node).

## Start here

::::{grid} 1 1 2 2

:::{card} Installation
:link: ./installation.md

Dependencies, environment and a first build. Start here to get a working
tree.
:::

:::{card} Basic concepts
:link: ./guide/guide_basic_concepts.md

GClass, gobj, yuno, events. The four words the rest of the documentation is
written in.
:::

:::{card} Design principles
:link: ./philosophy/design_principles.md

The engineering decisions behind the framework, and what each one buys and
costs.
:::

:::{card} Operating Yuneta
:link: ./operating_yuneta.md

Running systems: the agent, the yuno lifecycle, traces, IPC, realms, auth
and the treedb.
:::
::::

The full table of contents is in the left sidebar. The **API reference** is
organized there by subsystem: GObj, Helpers, Logging, Parsers, Timeranger2,
TLS, Event Loop and JavaScript.

## Whole documents

Some things do not fit a chapter. These are single pages, each one written to
be read from top to bottom.

::::{grid} 1 1 2 2

:::{card} Essay — High-level semantics, low-level language
:link: /high-semantics

Yuneta in one page: services, roles and messages without the runtime that
usually pays for them, with a bill next to every decision.
:::

:::{card} Interactive — The login, gobj by gobj
:link: /login-flow

The auth_bff exchange as a graph that runs, with the real gobjs and the real
events on the edges. Five scenarios.
:::

:::{card} Live — The shell, running
:link: https://demo.yuneta.io

Not a diagram of the UI library, the library itself, in your browser. Open
it on a phone too: the layouts change per breakpoint.
:::

:::{card} Field guide — Getting back
:link: /navigation

What the Back button must do in an application that is one page: history,
overlays, and never losing the reader.
:::

:::{card} Field guide — The file that disappears
:link: /package-transition

Why a package upgrade deletes a file it is supposed to keep, down to the
`%posttrans` that fixes it.
:::
::::

## Then

[Domain Model](philosophy/domain_model.md) gives the vocabulary the framework
uses to model reality — realms, entities, messages, CRUDLU. The
[Guides](guide/guide_gclass.md) show how to build with it, and
[Inspiration](philosophy/philosophy.md) is optional: the humanist angle that
shaped the vocabulary.
