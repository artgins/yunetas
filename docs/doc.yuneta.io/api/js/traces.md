---
title: 'JS: Traces'
description: >-
  Turning the machine trace on and off, per gclass and per gobj, and the
  silencing side that keeps a trace readable.
---

# Traces

The trace is the execution log of the framework. The `machine` level writes
every event that enters a state machine, so it shows what occurred and in which
order. It is the first tool for a browser application, and not the last.

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.13.5/src/gobj.js)

:::{important}
Never debug a gclass with a call to `console.log()`. A trace has a scope, a
standard format and a correlation between gobjs, and a `console.log()` has
none. If no level fits, add a level to the gclass.
:::

A gobj traces at the union of three masks: the global mask, the mask of its
gclass, and its own mask. Each of the three has a **silencing** partner, and the
silencing side wins.

---

## Levels

(js_trace_level_t)=
### [`trace_level_t`](https://github.com/artgins/gobj-js/blob/7.13.5/src/gobj.js#L400)

The global levels, with the name that every function below accepts.

| Name | Description |
|---|---|
| `machine` | Every event that enters a state machine. |
| `create_delete` | The creation and the destruction of a gobj. |
| `create_delete2` | The same, with the `kw`. |
| `subscriptions` | The subscriptions. |
| `start_stop` | The start and the stop of a gobj. |
| `ev_kw` | The payload of the events. |
| `authzs` | The authorizations. |
| `states` | Each change of state. |
| `gbuffers` | The buffers. |
| `timer` | The timers. |
| `fs` | The file system. |
| `liburing` | The io_uring mixins. The browser does not use it. |
| `timer_periodic` | The periodic timers. |
| `liburing_timer` | The io_uring timer. The browser does not use it. |
| `commands` | The commands. |

Every function accepts three forms for `level`: the name from this table, a bit
mask as a string of digits, or an empty value, which means every global level.

---

## Global

(js_gobj_set_global_trace)=
### [`gobj_set_global_trace(level, set)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/gobj.js#L616)

Turns a level on or off for every gclass. Returns `0`, or `-1` when the level
name does not exist.

```javascript
gobj_set_global_trace("machine", true);
```

(js_gobj_set_global_no_trace)=
### [`gobj_set_global_no_trace(level, set)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/gobj.js#L630)

Silences a level for every gclass. It wins against
[`gobj_set_global_trace()`](#js_gobj_set_global_trace).

(js_gobj_global_trace_level)=
### [`gobj_global_trace_level()`](https://github.com/artgins/gobj-js/blob/7.13.5/src/gobj.js#L644)

Gives the global mask, as a number.

(js_gobj_repr_global_trace_levels)=
### [`gobj_repr_global_trace_levels()`](https://github.com/artgins/gobj-js/blob/7.13.5/src/gobj.js#L745)

Gives the catalog of the global levels, as a list of records with `name`,
`bit`, `description` and `set`. A development panel builds its list of switches
from it.

(js_gobj_set_deep_trace)=
### [`gobj_set_deep_trace(value)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/gobj.js#L650)

Turns everything on at the same time, for a session that hunts something.
Returns `0`.

---

## Per gclass

(js_gobj_set_gclass_trace)=
### [`gobj_set_gclass_trace(gclass, level, set)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/gobj.js#L660)

Turns a level on or off for one gclass. `gclass` accepts the gclass itself or
its name, so a caller that holds no handle gives the name. Returns `0`, or `-1`
when the gclass or the level does not exist.

(js_gobj_set_gclass_no_trace)=
### [`gobj_set_gclass_no_trace(gclass, level, set)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/gobj.js#L707)

Silences a level for one gclass.

This is the pair that keeps a `machine` trace readable:

```javascript
gobj_set_gclass_no_trace("C_TIMER", "machine", true);
gobj_set_global_no_trace("timer_periodic", true);
```

The `machine` level traces every event by design, and a timer is an event. A
tick of one second buries what you follow, so silence the timers first.

---

## Per gobj

(js_gobj_set_gobj_trace)=
### [`gobj_set_gobj_trace(gobj, level, set)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/gobj.js#L679)

Turns a level on or off for one gobj. Returns `0`, or `-1` when `gobj` is empty.

(js_gobj_set_gobj_no_trace)=
### [`gobj_set_gobj_no_trace(gobj, level, set)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/gobj.js#L726)

Silences a level for one gobj.

(js_gobj_trace_level)=
### [`gobj_trace_level(gobj)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/gobj.js#L479)

Gives the mask in force for a gobj: the union of the global mask, the mask of
its gclass and its own. The C kernel computes it in the same way.

(js_gobj_trace_no_level)=
### [`gobj_trace_no_level(gobj)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/gobj.js#L494)

Gives the silencing mask in force for a gobj.

---

## The format of the machine trace

(js_gobj_set_trace_machine_format)=
### [`gobj_set_trace_machine_format(format)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/gobj.js#L364)

Chooses the format of the lines of the `machine` trace.

(js_gobj_trace_machine_format)=
### [`gobj_trace_machine_format()`](https://github.com/artgins/gobj-js/blob/7.13.5/src/gobj.js#L369)

Gives the format that is in force.

---

## Write a trace

The four writers are in [Logging](logging.md):
[`trace_msg()`](logging.md#js_trace_msg) and
[`trace_json()`](logging.md#js_trace_json) write a line and an object.
