# Philosophy

The ideas behind Yuneta: why it is built around typed graphs of
finite-state objects (GObjects) that communicate only through events, and
the model that shapes every design decision in the framework.

```{figure} ../_static/gobj_blackbox.svg
:alt: A gobj is a black box — an instance (its name) of a gclass (its role, e.g. C_TCP). Events are its communication channel, flowing in and out; commands, attributes and statistics are ports on its surface. Links between gobjs come later, with the typed graph.
:width: 100%

A **gobj** is a black box. It is an *instance* — identified by its **name**
(e.g. `"server-1"`) — of a **gclass**, the *role* it plays (e.g. `C_TCP`).
You never reach inside it. **Events** are its communication channel: it
receives them and sends them, and they are the *only* way gobjs talk to
each other. **Commands**, **attributes** and **statistics** are ports on
its surface — how *you* drive and inspect it. There are no links here yet:
how gobjs connect into a graph comes next, in the typed-graph model.
```

## In this section

- [Design Principles](design_principles.md) — the rules that keep the system
  simple, observable, and portable across languages.
- [The Typed-Graph Model](typed_graph_model.md) — nodes, links, and the typed
  hierarchy that represents both runtime structure and persisted data.
- [Domain Model](domain_model.md) — how a problem domain maps onto yunos,
  gclasses, and events.
- [Inspiration](philosophy.md) — the prior art and motivations Yuneta draws
  from.
