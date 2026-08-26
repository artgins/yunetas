---
title: 'JS: GClass Registration'
description: >-
  Registering a gclass, its finite state machine, its methods and its
  tables, and the functions that build one at run time.
---

# GClass Registration

A **gclass** is a definition that the application registers one time at start
up. It carries the schema of the attributes, the finite state machine, the
methods of the lifecycle, the commands and the authorizations.

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js)

---

(js_GObj)=
## [`GObj`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L202)

The class of an instance. [`is_gobj()`](helpers_json.md#js_is_gobj) tells if a
value is one. Application code does not build one with `new`. It calls a
creation function of the [lifecycle](lifecycle.md).

---

(js_gclass_create)=
## [`gclass_create()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L1104)

Registers a gclass.

```javascript
gclass_create(
    gclass_name,        // "C_MY_CLASS"
    event_types,        // [[event_name, flag], …]
    states,             // [[state_name, ev_action_table], …]
    gmt,                // the methods of the gclass (mt_create, mt_start, …)
    lmt,                // the local methods
    attrs_table,        // the array of SDATA
    priv,               // the private data, copied for each instance
    authz_table,        // or 0
    command_table,      // or 0
    s_user_trace_level,
    gclass_flag
)
```

**Returns**

The gclass, or `null` when the registration fails.

:::{warning}
`attrs_table` must be a real table, and a gclass with no attributes gives a
table that holds only [`SDATA_END()`](commands.md#js_SDATA_END). A value of `0`
there passes the registration, and the creation of the first instance fails.
:::

(js_gclass_flag_t)=
## [`gclass_flag_t`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L248)

The flags of a gclass.

| Flag | Description |
|---|---|
| `gcflag_manual_start` | [`gobj_start_tree()`](lifecycle.md#js_gobj_start_tree) does not start it. |
| `gcflag_no_check_output_events` | A publication does not check the list of the output events. |
| `gcflag_ignore_unknown_attrs` | A creation ignores an attribute that the schema does not hold. |
| `gcflag_required_start_to_play` | A play needs a start first. |
| `gcflag_singleton` | There is one instance only. |

(js_gclass_find_by_name)=
## [`gclass_find_by_name(gclass_name, verbose)`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L1477)

Finds a gclass by its name. With `verbose` set to `true` the function writes a
log error when the gclass does not exist.

(js_gclass_unregister)=
## [`gclass_unregister(gclass)`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L1218)

Takes a gclass out of the register. It accepts the gclass or its name.

(js_gclass_check_fsm)=
## [`gclass_check_fsm(gclass)`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L1384)

Checks the finite state machine of a gclass, and gives the quantity of the
errors that it finds. It finds an action that goes to a state that does not
exist, and an event that no state declares.

---

## Build a gclass at run time

These four functions build a gclass one piece at a time. Application code rarely
needs them. A generator of code and a dynamic gclass use them.

(js_gclass_add_state)=
### [`gclass_add_state(gclass, state_name)`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L1281)

Adds one state.

(js_gclass_add_ev_action)=
### [`gclass_add_ev_action(gclass, state_name, event_name, action, next_state)`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L1301)

Adds one row of `(event, action, next state)` to a state.

(js_gclass_add_event_type)=
### [`gclass_add_event_type(gclass, event_name, event_flag)`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L1348)

Adds one event to the list of the events of the gclass.

(js_gclass_event_type)=
### [`gclass_event_type(gclass, event_name)`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L1361)

Gives the record of an event of the gclass, with its flags.

---

## The action function

```javascript
function ac_connect(gobj, event, kw, src) {
    // event = "EV_CONNECT"
    // kw    = the JSON payload, such as { url: "ws://…" }
    // src   = the gobj that sent the event
    //
    // Give 0 back on success, and -1 on failure.
    return 0;
}
```

See [Writing a Custom GClass](../../guide/guide_gclass.md) for a full example.

---

## The subscription model

Each gclass takes one of two models, and writes the block in `mt_create`. Do not
invent a third one.

```javascript
/*
 *  CHILD subscription model
 */
let subscriber = gobj_read_pointer_attr(gobj, "subscriber");
if(!subscriber) {
    subscriber = gobj_parent(gobj);
}
gobj_subscribe_event(gobj, null, {}, subscriber);
```

```javascript
/*
 *  SERVICE subscription model
 */
const subscriber = gobj_read_pointer_attr(gobj, "subscriber");
if(subscriber) {
    gobj_subscribe_event(gobj, null, {}, subscriber);
}
```

A **child** goes to its parent, and the parent declares every event that the
child publishes. A **service** goes to the subscribers that ask for it.

When a publication of a child gives *"Event NOT DEFINED in state"*, add the
event to the state machine of the parent, or make the gobj a service. Never take
the parent out of the child model to make the message quiet.
