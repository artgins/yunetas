# GClass Registration

A **GClass** is a class definition registered once at startup. It
declares its attribute schema, FSM (states and event/action tables),
lifecycle methods, commands, and authz rules.

## Registration

```javascript
gclass_create(
    name,               // "C_MY_CLASS"
    event_types,        // [[event_name, flag], ...]
    states,             // [[state_name, ev_action_table], ...]
    gmt,                // gclass methods table (mt_create, mt_start, …)
    lmt,                // low-level methods table
    attrs_table,        // SDATA array
    private_data,       // template object copied per instance
    authz_table,        // or 0
    command_table,      // or 0
    s_user_trace_level,
    gclass_flag
)
```

## Lookup and introspection

```javascript
gclass_find_by_name(name)        // → GClass or null
gclass_check_fsm(gclass)         // validate FSM (returns error count)
```

## Runtime mutation (advanced)

```javascript
gclass_add_event_type(gclass, event_name, flag)
gclass_add_state(gclass, state_name)
gclass_add_ev_action(gclass, state_name, event_name, action_fn, next_state)
```

These are rarely needed in application code — they exist so code
generators and dynamic GClasses can build up a gclass incrementally.

## Typical action function

```javascript
function ac_connect(gobj, event, kw, src) {
    // event = "EV_CONNECT"
    // kw    = JSON payload ({ url: "ws://..." })
    // src   = the gobj that sent the event
    //
    // return 0 to consume kw, -1 on error.
    return 0;
}
```

See [Writing a Custom GClass](../../guide/guide_gclass.md) in the
guides section for a full worked example.
