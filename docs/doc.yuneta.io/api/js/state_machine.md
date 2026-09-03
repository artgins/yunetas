---
title: 'JS: State Machine'
description: >-
  The table of states of a gclass, the change of state at run time and
  the flags of an event.
---

# State Machine

Each gclass declares a finite state machine: a list of states, and for each
state a table of rows with the form `(event, action, next state)`.

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.16.2/src/gobj.js)

```javascript
const st_idle = [
    ["EV_CONNECT",    ac_connect,    "ST_CONNECTED"],
    ["EV_TIMEOUT",    ac_timeout,    null],           // null = stay in this state
];
const st_connected = [
    ["EV_DISCONNECT", ac_disconnect, "ST_IDLE"],
    ["EV_MESSAGE",    ac_message,    null],
];

const states = [
    ["ST_IDLE",      st_idle],
    ["ST_CONNECTED", st_connected],
];
```

When an event arrives, the framework reads the state that is active, finds the
row of the event, calls the action, and goes to the next state. With a next
state of `null` the gobj stays where it is.

---

(js_gobj_current_state)=
## [`gobj_current_state(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/gobj.js#L3659)

Gives the name of the state that is active.

(js_gobj_change_state)=
## [`gobj_change_state(gobj, state_name)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/gobj.js#L3608)

Changes the state without an event. Returns `true` when the state changes.

(js_gobj_has_event)=
## [`gobj_has_event(gobj, event, event_flag)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/gobj.js#L3761)

Tells if the gclass declares an event. Give `0` in `event_flag` to accept every
flag.

(js_gobj_has_output_event)=
## [`gobj_has_output_event(gobj, event, event_flag)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/gobj.js#L3778)

Tells if the gclass declares an event as an output event.

---

(js_event_flag_t)=
## [`event_flag_t`](https://github.com/artgins/gobj-js/blob/7.16.2/src/gobj.js#L297)

The flags of an event, in the table `event_types` of the gclass.

| Flag | Description |
|---|---|
| `EVF_NO_WARN_SUBS` | A publication with no subscriber writes no warning. |
| `EVF_OUTPUT_EVENT` | The gclass publishes this event. |
| `EVF_SYSTEM_EVENT` | An event of the framework. |

```javascript
const event_types = [
    ["EV_TIMEOUT_PERIODIC", event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS],
    [null, 0]
];
```

Use `EVF_NO_WARN_SUBS` when a subscriber is optional. It is the annotation that
says *"no subscriber is not a fault"*, and it is not a way to make a message
quiet. Keep the warning when the gclass really needs somebody to answer.

---

## An event that the state does not declare

The framework writes an error. That is deliberate.

:::{important}
Never add an action that does nothing to make *"Event NOT DEFINED in state"*
quiet. The message shows a state machine that is not complete, or a sender that
emits in the wrong situation. Find who sends the event in that state, and repair
that path.
:::

A state is also the way to say that something is not ready. A view whose whole
life happens in one state, with a guard such as `if(!priv.x) return`, hides the
fault: the button does nothing and nobody hears about it. Model the situation as
a state, and the wrong event fails with the name of its sender.
