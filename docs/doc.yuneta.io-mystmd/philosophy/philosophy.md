# Inspiration

Yuneta is an engineering framework, but its vocabulary — *events*,
*actions*, *gobjs*, *hierarchy*, [*realms*](domain_model.md#realms) —
did not come out of a specification document. It came from looking at
how **life itself** is organised and borrowing the words that fit.

This page is the short, non-technical companion to the
[Design Principles](design_principles.md) and the
[Domain Model](domain_model.md). It is optional: nothing here is
required to build with Yuneta. Read it if you want to understand
**why the framework uses the words it uses**.

## Events are what make anything happen

> *Without events, nothing happens.*

Every change — in a program, in a conversation, in a life — is
triggered by an event. Between events there is only waiting.
Yuneta takes this literally: a gobj does nothing until an event
reaches it, and every event is either consumed, transformed, or
forwarded.

Events come in two flavours, and both are present in the framework
the same way they are present in life:

- **Internal events** — self-generated. They represent will, agency,
  the potential to act. A timer firing, a gobj deciding to publish,
  a periodic heartbeat.
- **External events** — coming from the environment. They represent
  the part we do not control. A packet on a socket, a signal from the
  OS, a user command, a filesystem change.

A well-designed gobj, like a well-designed life, is one that reacts
gracefully to external events while still generating enough internal
ones to make progress.

## What is above is below; what is inside is outside

A Yuneta system is organised as a **tree of gobjs**, and that tree
has the same shape at every scale: a yuno contains gobjs; a gobj can
contain child gobjs; two yunos can be connected as if they were a
single bigger tree. The interaction patterns — parent to child,
service to client, publisher to subscriber — repeat at every level.

The outside of a gobj (its events, commands, attributes) mirrors its
inside (its state machine, private data, action callbacks). The
outside of a yuno (its control plane, its statistics, its persisted
history) mirrors the outside of each gobj it contains. This
fractal-like consistency is the reason a single set of tools —
`ycommand`, tracing, logging, persistence — works at every scale.

## Time is the axis everything hangs from

Events happen **in order**, and Yuneta never forgets the order.
Persistence is append-only and indexed by monotonically growing row
ids. Logs, traces, queues, message stores, and the graph database are
all views over the same time-ordered stream. Nothing is ever
overwritten without leaving a trace.

This is also why **consistency** and **repetition** matter in Yuneta.
A service that does the same thing the same way, event after event,
for months, is a service you can trust, reason about, and replay.
Sporadic, unpredictable behaviour is the enemy.

## Hierarchy gives lifetimes their meaning

Every gobj has exactly one parent, and the root of every yuno is
itself a gobj. This is not only an implementation convenience: it is
a statement that nothing in the system exists without a context and a
lifetime. When a parent goes, its children go with it. When a branch
is paused, everything in it is paused. When you trace a log line, you
always know the full lineage of the gobj that produced it.

A well-designed Yuneta application, like a well-designed organisation,
knows who is responsible for what, who creates whom, and when things
are allowed to end.

## A final note

Yuneta is a tool. It is written in C (and JavaScript), it runs on
Linux, and it solves a fairly concrete class of engineering problems.
But the reason it uses the words it uses — events, actions, hierarchy,
realms, time — is that those words describe how reality is organised,
and reality is the system we are ultimately modelling.

If any of this resonates, read the [Design Principles](design_principles.md)
next — it translates every idea on this page into a concrete
engineering decision you can measure and test.

## Where to go next

- [Design Principles](design_principles.md) — the same ideas expressed
  as engineering trade-offs.
- [Domain Model](domain_model.md) — the vocabulary used to model real
  systems.
- [Basic Concepts](../guide/basic_concepts.md) — GClass, gobj, yuno
  from the code side.
