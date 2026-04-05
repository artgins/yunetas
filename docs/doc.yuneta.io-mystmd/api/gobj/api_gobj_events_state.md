# Events/State Functions

Source code in:
- [gobj.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)
- [gobj.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)

(gobj_change_state)=
## `gobj_change_state()`

Changes the current state of the given `hgobj` to the specified `state_name`. If the new state is different from the current state, it updates the state and publishes the [`EV_STATE_CHANGED`](#EV_STATE_CHANGED) event.

```C
BOOL gobj_change_state(
    hgobj gobj,
    gobj_state_t state_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` whose state is to be changed. |
| `state_name` | `gobj_state_t` | The new state to transition to. |

**Returns**

Returns `TRUE` if the state was changed successfully, otherwise returns `FALSE`.

**Notes**

If the new state is the same as the current state, no change occurs. If the `hgobj` has a `mt_state_changed` method, it will be called instead of publishing [`EV_STATE_CHANGED`](#EV_STATE_CHANGED).

---

(gobj_current_state)=
## `gobj_current_state()`

Retrieves the current state of the given `hgobj`.

```C
gobj_state_t gobj_current_state(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `hgobj` | `hgobj` | The `hgobj` whose current state is to be retrieved. |

**Returns**

Returns the current state of the `hgobj` as a `gobj_state_t` string.

**Notes**

If `hgobj` is `NULL`, an error is logged, and an empty string is returned.

---

(gobj_event_type)=
## `gobj_event_type()`

Retrieves the event type information for a given event in the specified gobj. If the event is not found in the gobj's event list, it checks the global event list if `include_system_events` is set to true.

```C
event_type_t *gobj_event_type(
    hgobj gobj,
    gobj_event_t event,
    BOOL include_system_events
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_` | `hgobj` | The gobj in which to search for the event. |
| `event` | `gobj_event_t` | The event whose type information is to be retrieved. |
| `include_system_events` | `BOOL` | If true, the function also checks the global event list for the event. |

**Returns**

A pointer to the `event_type_t` structure describing the event if found, or NULL if the event is not found.

**Notes**

If `include_system_events` is set to true, the function will also check the global event list for system events.

---

(gobj_event_type_by_name)=
## `gobj_event_type_by_name()`

Retrieves the event type information for a given event name in the specified gobj.

```C
event_type_t *gobj_event_type_by_name(
    hgobj gobj,
    const char *event_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The gobj instance in which to search for the event. |
| `event_name` | `const char *` | The name of the event to look up. |

**Returns**

A pointer to the `event_type_t` structure if the event is found, otherwise `NULL`.

**Notes**

This function searches for the event in the gobj's event list and the global event list. If the event is not found, it returns `NULL`.

---

(gobj_has_event)=
## `gobj_has_event()`

Checks if the given `gobj` supports the specified event, optionally filtering by event flags.

```C
BOOL gobj_has_event(
    hgobj gobj,
    gobj_event_t event,
    event_flag_t event_flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `gobj` instance to check. |
| `event` | `gobj_event_t` | The event to check for. |
| `event_flag` | `event_flag_t` | Optional flags to filter the event check. |

**Returns**

Returns `TRUE` if the event exists in the `gobj`'s event list and matches the given flags, otherwise returns `FALSE`.

**Notes**

This function does not differentiate between input and output events. Use [`gobj_has_output_event()`](#gobj_has_output_event) to specifically check for output events.

---

(gobj_has_output_event)=
## `gobj_has_output_event()`

Checks if the given `gobj` has the specified `event` in its output event list, optionally filtered by `event_flag`.

```C
BOOL gobj_has_output_event(
    hgobj gobj,
    gobj_event_t event,
    event_flag_t event_flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance to check. |
| `event` | `gobj_event_t` | The event to check for in the output event list. |
| `event_flag` | `event_flag_t` | Optional flag to filter the event check. |

**Returns**

Returns `TRUE` if the event exists in the output event list and matches the given `event_flag`, otherwise returns `FALSE`.

**Notes**

This function is useful for verifying if a GObj can publish a specific event before attempting to do so.

---

(gobj_has_state)=
## `gobj_has_state()`

Checks if the given `gobj` has the specified state in its state machine.

```C
BOOL gobj_has_state(
    hgobj gobj, 
    gobj_state_t state
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `gobj` instance to check. |
| `state` | `gobj_state_t` | The state to verify in the `gobj`'s state machine. |

**Returns**

Returns `TRUE` if the `gobj` has the specified state, otherwise returns `FALSE`.

**Notes**

This function verifies if the given state exists in the state machine of the `gobj`. It does not check if the `gobj` is currently in that state.

---

(gobj_in_this_state)=
## `gobj_in_this_state()`

Checks if the given `hgobj` is currently in the specified state.

```C
BOOL gobj_in_this_state(
    hgobj gobj,
    gobj_state_t state
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance to check. |
| `state` | `gobj_state_t` | The state to compare against the current state of `gobj`. |

**Returns**

Returns `TRUE` if `gobj` is in the specified `state`, otherwise returns `FALSE`.

**Notes**

This function is useful for verifying the current state of a `hgobj` before performing state-dependent operations.

---

(gobj_send_event)=
## `gobj_send_event()`

The `gobj_send_event` function processes an event in the given destination gobj, executing the corresponding action in its current state.

```C
int gobj_send_event(
    hgobj        dst,
    gobj_event_t event,
    json_t       *kw,
    hgobj        src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `dst` | `hgobj` | The destination gobj that will process the event. |
| `event` | `gobj_event_t` | The event to be processed. |
| `kw` | `json_t *` | A JSON object containing event-specific data. The ownership is transferred to the function. |
| `src` | `hgobj` | The source gobj that is sending the event. |

**Returns**

Returns 0 on success, -1 if the event is not defined in the current state, or if an error occurs.

**Notes**

If the event is not found in the current state of `dst`, the function checks if `dst` has a custom event injection method (`mt_inject_event`). If defined, it delegates event processing to that method.

---

(gobj_find_event_type)=
## `gobj_find_event_type()`

*Description pending — signature extracted from header.*

```C
event_type_t *gobj_find_event_type(
    const char *event,
    event_flag_t event_flag,
    BOOL verbose
);
```

---

(gobj_send_event_to_children)=
## `gobj_send_event_to_children()`

*Description pending — signature extracted from header.*

```C
int gobj_send_event_to_children(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,
    hgobj src
);
```

---

(gobj_send_event_to_children_tree)=
## `gobj_send_event_to_children_tree()`

*Description pending — signature extracted from header.*

```C
int gobj_send_event_to_children_tree(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,
    hgobj src
);
```

---

(gobj_state_find_by_name)=
## `gobj_state_find_by_name()`

*Description pending — signature extracted from header.*

```C
hgclass gobj_state_find_by_name(
    gclass_name_t gclass_name
);
```

---

