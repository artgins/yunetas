---
title: Yuneta Simplified
subtitle: An Asynchronous Development Framework
---

::::{grid} 1 1 2 2
:gutter: 3

:::{grid-item}
:columns: 12 12 4 4

```{image} _static/yuneta-image.svg
:width: 200px
:align: center
```

:::

:::{grid-item}
:columns: 12 12 8 8

`Yuneta Simplified` is a **development framework** focused on **messaging** and
**services**, based on
[Event-driven](https://en.wikipedia.org/wiki/Event-driven_programming),
[Automata-based](https://en.wikipedia.org/wiki/Automata-based_programming) and
[Object-oriented](https://en.wikipedia.org/wiki/Object-oriented_programming)
programming paradigms. Heavy use of JSON, **time-series**, **key-value**,
**flat-files** and **graph** concepts.

[Yuneta Simplified](https://yuneta.io) is a **real-time system** (RTS) that
includes **development**, **testing**, and **deployment** features. Built for
Linux and **deployable** on any **bare-metal** server.

For [Linux](https://en.wikipedia.org/wiki/Linux). Versions in C, partially in
JavaScript.

:::

::::

---

## About this pilot

:::{note}
This site is a **pilot migration** of the original Sphinx documentation at
`docs/doc.yuneta.io/` to [mystmd (Jupyter Book 2)](https://mystmd.org).
It does **not** contain all the pages — only a representative slice is
included so we can compare stacks side by side.

**Key difference from the Quarto pilot**: the source files in
`philosophy/` and `api/helpers/istream/` are **copied verbatim** from the
Sphinx site. Not one character has been changed. mystmd reads MyST
Markdown natively, so every `{tab-set}`, `{tab-item}`, `{list-table}`,
`{dropdown}` directive works as-is.
:::

## Explore the pilot

::::{grid} 1 1 3 3
:gutter: 3

:::{grid-item-card} Philosophy
:link: philosophy/philosophy.md

The design principles behind Yuneta: events, actions, hierarchy,
time and data.
:::

:::{grid-item-card} istream API
:link: api/helpers/istream/index.md

Input-stream helper: lifecycle, reading primitives, buffer inspection.
**17 reference pages shared verbatim** with the Sphinx site.
:::

:::{grid-item-card} Demo notebook
:link: notebooks/demo_timeseries.md

A small executable MyST document with a Python code cell — the native
replacement for the old `myst_nb` Sphinx extension.
:::

::::
