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

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js)

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
### [`trace_level_t`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L406)

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
### [`gobj_set_global_trace(level, set)`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L631)

Turns a level on or off for every gclass. Returns `0`, or `-1` when the level
name does not exist.

```javascript
gobj_set_global_trace("machine", true);
```

(js_gobj_set_global_no_trace)=
### [`gobj_set_global_no_trace(level, set)`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L645)

Silences a level for every gclass. It wins against
[`gobj_set_global_trace()`](#js_gobj_set_global_trace).

(js_gobj_global_trace_level)=
### [`gobj_global_trace_level()`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L659)

Gives the global mask, as a number.

(js_gobj_global_trace_no_level)=
### [`gobj_global_trace_no_level()`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L664)

Gives the global silencing mask, as a number.

(js_gobj_set_global_trace2)=
### [`gobj_set_global_trace2(bitmask, set)`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L670)

Turns global levels on or off by bit mask instead of by name, like the C
function of the same name. `0xFFFFFFFF` with `set` false clears every global
level. Returns `0`.

```javascript
gobj_set_global_trace2(trace_level_t.TRACE_MACHINE | trace_level_t.TRACE_EV_KW, true);
```

(js_gobj_set_global_no_trace2)=
### [`gobj_set_global_no_trace2(bitmask, set)`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L680)

The silencing partner of [`gobj_set_global_trace2()`](#js_gobj_set_global_trace2).

(js_gobj_get_global_trace_level)=
### [`gobj_get_global_trace_level()`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L695)

Gives the global levels in force, as a list of names. It is what the yuno saves
for the global scope.

```javascript
gobj_get_global_trace_level();    // ["machine", "start_stop"]
```

(js_gobj_get_global_trace_no_level)=
### [`gobj_get_global_trace_no_level()`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L700)

Gives the global silencing levels in force, as a list of names.

(js_gobj_repr_global_trace_levels)=
### [`gobj_repr_global_trace_levels()`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L833)

Gives the catalog of the global levels, as a list of records with `name`,
`bit`, `description` and `set`. A development panel builds its list of switches
from it.

(js_gobj_set_deep_trace)=
### [`gobj_set_deep_trace(value)`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L736)

Turns everything on at the same time, for a session that hunts something.
Returns `0`.

---

## Per gclass

(js_gobj_set_gclass_trace)=
### [`gobj_set_gclass_trace(gclass, level, set)`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L746)

Turns a level on or off for one gclass. `gclass` accepts the gclass itself or
its name, so a caller that holds no handle gives the name. Returns `0`, or `-1`
when the gclass or the level does not exist.

(js_gobj_get_gclass_trace_level)=
### [`gobj_get_gclass_trace_level(gclass)`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L705)

Gives the levels in force for a gclass, as a list of names: its own levels and
the global ones, as in the C kernel. The names of the gclass's own levels come
from the `s_user_trace_level` it gives to `gclass_create()`, a list of
`[name, description]` in bit order.

(js_gobj_get_gclass_trace_level2)=
### [`gobj_get_gclass_trace_level2(gclass)`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L715)

Gives the gclass's own levels only, without the global ones. It is what the
yuno saves for the scope of a gclass.

```javascript
gobj_set_gclass_trace("C_IEVENT_CLI", "ievents", true);
gobj_get_gclass_trace_level2("C_IEVENT_CLI");   // ["ievents"]
```

(js_gobj_get_gclass_trace_no_level)=
### [`gobj_get_gclass_trace_no_level(gclass)`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L725)

Gives the silencing levels of a gclass, as a list of names.

(js_gobj_set_gclass_no_trace)=
### [`gobj_set_gclass_no_trace(gclass, level, set)`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L794)

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
### [`gobj_set_gobj_trace(gobj, level, set)`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L766)

Turns a level on or off for one gobj. Returns `0`, or `-1` when `gobj` is empty.

(js_gobj_set_gobj_no_trace)=
### [`gobj_set_gobj_no_trace(gobj, level, set)`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L814)

Silences a level for one gobj.

(js_gobj_trace_level)=
### [`gobj_trace_level(gobj)`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L519)

Gives the mask in force for a gobj: the union of the global mask, the mask of
its gclass and its own. The C kernel computes it in the same way.

(js_gobj_trace_no_level)=
### [`gobj_trace_no_level(gobj)`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L534)

Gives the silencing mask in force for a gobj.

---

## Persisted by the yuno

`C_YUNO` keeps the levels that a user sets, in its persistent attributes
`trace_levels` and `no_trace_levels`, and restores them in its `mt_create`. It
is the model of the C kernel: the same attributes, the same commands and the
same keys.

| Command | Parameters | Key that it saves |
|---|---|---|
| `set-global-trace` | `level`, `set` | `trace_levels.__global_trace__` |
| `set-global-no-trace` | `level`, `set` | `no_trace_levels.__global_no_trace__` |
| `set-gclass-trace` | `gclass_name`, `level`, `set` | `trace_levels.<gclass>` |
| `set-gclass-no-trace` | `gclass_name`, `level`, `set` | `no_trace_levels.<gclass>` |

`get-global-trace`, `get-global-no-trace`, `get-gclass-trace` and
`get-gclass-no-trace` read the same levels. `set` accepts `1` / `0`, `true` /
`false` and `set` / `reset`.

```javascript
gobj_command(gobj_yuno(), "set-gclass-trace",
    {gclass_name: "C_IEVENT_CLI", level: "ievents", set: 1}, gobj_yuno());
```

A command saves its **whole scope**, from the levels in force, and an empty
scope is saved as `[]`. At start up, **a saved scope replaces** what `main()`
set before it created the yuno. A scope that was never saved keeps the default
of `main()`.

```javascript
// main.js: the defaults
gobj_set_global_no_trace("timer_periodic", true);

// the user turns the periodic timer back on, once
gobj_command(gobj_yuno(), "set-global-no-trace",
    {level: "timer_periodic", set: 0}, gobj_yuno());
// saved: no_trace_levels = {"__global_no_trace__": []}
// every later start up: timer_periodic is NOT silenced
```

The yuno persists through the functions that the app gives to
`gobj_start_up()`. An app that gives none keeps the levels until the page
reloads.

The traffic of the websocket is the level `ievents` (or `ievents2`) of
`C_IEVENT_CLI`. Its lines go to the function in the yuno attribute
`trace_ievent_callback`, or to the console when that attribute is empty.

---

## The format of the machine trace

(js_gobj_set_trace_machine_format)=
### [`gobj_set_trace_machine_format(format)`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L370)

Chooses the format of the lines of the `machine` trace.

(js_gobj_trace_machine_format)=
### [`gobj_trace_machine_format()`](https://github.com/artgins/gobj-js/blob/7.25.3/src/gobj.js#L375)

Gives the format that is in force.

---

## Write a trace

The four writers are in [Logging](logging.md):
[`trace_msg()`](logging.md#js_trace_msg) and
[`trace_json()`](logging.md#js_trace_json) write a line and an object.
