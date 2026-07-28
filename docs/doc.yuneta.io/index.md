---
title: Yuneta Simplified
---

# Yuneta Simplified

::::{grid} 1 1 2 2

:::{grid-item}

## *An Asynchronous Development Framework*

**Current version: [7.9.2](https://github.com/artgins/yunetas/tree/7.9.2)**

*Documentation updated: 2026-07-28*
:::

:::{grid-item}

```{image} ./_static/yuneta-image.svg
:width: 150px
:align: center
```
:::
::::

`Yuneta Simplified` is a **development framework** focused on **messaging** and **services**, based on
[Event-driven](https://en.wikipedia.org/wiki/Event-driven_programming),
[Automata-based](https://en.wikipedia.org/wiki/Automata-based_programming)
and [Object-oriented](https://en.wikipedia.org/wiki/Object-oriented_programming)
programming paradigms.
Heavy use of JSON, **time-series**, **key-value**, **flat-files** and **graphs** concepts.

[Yuneta Simplified](https://yuneta.io) is a **real-time system** (RTS) that includes **development**, **testing**, and **deployment** features. Built for Linux, and **deployable** on any **bare-metal** server.

Yuneta produces **fully static binaries** by default — no shared libraries, no dynamic linker. A compiled yuno can be copied to any Linux machine of the same CPU architecture and run immediately, with zero dependencies to install.

Specialized in IoT data collection, all types of devices, and data exchange and protocol adaptation between systems, including collection, **publication/subscription**, and querying of **messages** in **real time**, with **historical** data storage.

The messages (**encrypted** or plain text) circulating within the Yuneta system can be persistent on disk or exist only while in transit or in the memory of a service. All data in JSON.

For [Linux](https://en.wikipedia.org/wiki/Linux).

Versions in **C** (reference implementation) and **JavaScript** (browser/Node).

---

## Interactive walkthroughs

Pages that run rather than read: a diagram you step through instead of a
figure you stare at. They live outside the sidebar's table of contents —
they are whole documents, not chapters — so this is where they are
listed.

- [**The login, gobj by gobj**](https://doc.yuneta.io/login-flow) — the
  `auth_bff` exchange as a graph that runs, with the real gobjs and the
  real events on the edges. Five scenarios (login, restore, silent
  refresh, transient failure, logout), the browser's cookie jar and the
  client FSM updating at every step, and the checklist a new GUI has to
  implement. Prose behind it: [Yuno auth](yuno-auth).

:::{note} Adding one
Drop a self-contained `index.html` under `docs/doc.yuneta.io/<slug>/`
(no external requests — the theme travels through
`localStorage["myst:theme"]`), install it in `deploy.sh` next to the
others, and add a line here. `myst` copies no raw files, which is why
the install step exists at all.
:::

---

## About this documentation

This site is built with [mystmd (Jupyter Book 2)](https://mystmd.org). The
full table of contents is in the left sidebar — use it to browse by
section.

Suggested reading order:

1. [Design Principles](philosophy/design_principles.md) — the
   engineering decisions behind Yuneta and what they buy / cost.
2. [Domain Model](philosophy/domain_model.md) — the vocabulary the
   framework uses to model reality (realms, entities, messages, CRUDLU).
3. [Installation](installation.md) — get a working build environment.
4. [Basic concepts](guide/guide_basic_concepts.md) — GClass, gobj, yuno,
   events.
5. [Guides](guide/guide_gclass.md) — how to actually build with them.
6. The **API reference** sections in the sidebar — organised by
   subsystem (GObj, Helpers, Logging, Parsers, Timeranger2, TLS,
   Event Loop, JavaScript).
7. [Inspiration](philosophy/philosophy.md) — optional, the humanist
   angle that shaped the framework's vocabulary.
