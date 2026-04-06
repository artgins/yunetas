# State Machine

Each GClass defines a finite state machine: a list of states, and per
state a table of `(event, action, next_state)` rows.

## State table

```javascript
const st_idle = [
    ["EV_CONNECT",    ac_connect,    "ST_CONNECTED"],
    ["EV_TIMEOUT",    ac_timeout,    null],           // null = stay in state
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

## Runtime API

```javascript
gobj_current_state(gobj)                         // → current state name (string)
gobj_change_state(gobj, new_state)               // force transition
gobj_has_event(gobj, event, event_flag)          // → boolean; pass 0 to ignore flag
gobj_has_output_event(gobj, event, event_flag)   // checks EVF_OUTPUT_EVENT
```

When an event is sent to a GObject, the runtime looks up the current
state, matches the event, calls the action function, then transitions
to `next_state` (or stays if `next_state` is `null`).

:::{tip}
If an event is sent that the current state does not handle, the
framework logs a warning by default. This is usually a sign of a
missing row in the state table — not a bug in the sender.
:::
