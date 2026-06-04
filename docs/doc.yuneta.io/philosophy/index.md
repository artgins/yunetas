# Philosophy

The ideas behind Yuneta: why it is built around typed graphs of
finite-state objects (GObjects) that communicate only through events, and
the model that shapes every design decision in the framework.

```{figure} ../_static/gobj_blackbox.svg
:alt: A gobj is a black box exposing one typed interface with four facets — events (messages in and out), commands (control-plane verbs), attributes (typed config and state) and statistics (live counters). The same contract holds for every gobj, in C and JavaScript.
:width: 100%

A **gobj** is a black box: you never reach inside it, you talk to it only
through one typed interface with four facets — **events** (the messages it
sends and receives), **commands** (its control-plane verbs), **attributes**
(its typed config and state) and **statistics** (its live counters). Every
component in Yuneta, in C and in JavaScript, is a gobj with this same
contract — and gobjs talk to *each other* only through events.
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
