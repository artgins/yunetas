---
title: Yuneta Simplified
---

# Yuneta Simplified

::::{grid}
:reverse:
:gutter: 2 1 1 1
:margin: 4 4 1 1

:::{grid-item}
:columns: 4

```{image} ./_static/yuneta-image.svg
:width: 150px
```
:::

:::{grid-item}
:columns: 8
:class: sd-fs-3

An Asynchronous Development Framework

:::

::::

`Yuneta Simplified` is a **development framework** focused on **messaging** and **services**, based on
[Event-driven](https://en.wikipedia.org/wiki/Event-driven_programming),
[Automata-based](https://en.wikipedia.org/wiki/Automata-based_programming)
and [Object-oriented](https://en.wikipedia.org/wiki/Object-oriented_programming)
programming paradigms.
Heavy use of JSON, **time-series**, **key-value**, **flat-files** and **graphs** concepts.

[Yuneta Simplified](https://yuneta.io) is a **real-time system** (RTS) that includes **development**, **testing**, and **deployment** features. Built for Linux, and **deployable** on any **bare-metal** server.

Specialized in IoT data collection, all types of devices, and data exchange and protocol adaptation between systems, including collection, **publication/subscription**, and querying of **messages** in **real time**, with **historical** data storage.

The messages (**encrypted** or plain text) circulating within the Yuneta system can be persistent on disk or exist only while in transit or in the memory of a service. All data in JSON.

For [Linux](https://en.wikipedia.org/wiki/Linux).

Versions in **C** (reference implementation) and **JavaScript** (browser/Node).

---

## About this documentation

This site is built with [mystmd (Jupyter Book 2)](https://mystmd.org). The
full table of contents is in the left sidebar — use it to browse by
section. Every page is authored in MyST Markdown, the same format the
previous Sphinx site used, so sources migrated one-to-one.

Start with:

- [Installation](installation.md) — get a working build environment.
- [Philosophy](philosophy/philosophy.md) — why Yuneta is shaped the way it is.
- [Basic concepts](guide/basic_concepts.md) — GClasses, GObjects, events.
- The **API reference** sections in the sidebar — organised by subsystem.
