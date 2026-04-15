# GClass

Register and introspect GClasses — the class descriptors that define a gobj's states, events, attributes, commands, and action callbacks. A GClass must be registered before any gobj of that class can be created.

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)

(gclass2json)=
## `gclass2json()`

Converts a given `hgclass` object into a JSON representation, including its attributes, commands, methods, and trace levels.

```C
json_t *gclass2json(
    hgclass gclass
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass_` | `hgclass` | The `hgclass` object to be converted into JSON. |

**Returns**

A `json_t *` object containing the JSON representation of the `hgclass`. The caller is responsible for managing the returned JSON object.

**Notes**

This function provides a structured JSON output of a `hgclass`, including its attributes, commands, methods, and trace levels. It is useful for debugging and introspection.

---

(gclass_add_ev_action)=
## `gclass_add_ev_action()`

Adds an event-action pair to a specified state in the given `hgclass`. The function associates an event with an action function and a next state transition.

```C
int gclass_add_ev_action(
    hgclass         gclass,
    gobj_state_t    state_name,
    gobj_event_t    event,
    gobj_action_fn  action,
    gobj_state_t    next_state
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The handle to the `gclass` where the event-action pair will be added. |
| `state_name` | `gobj_state_t` | The name of the state to which the event-action pair will be added. |
| `event` | `gobj_event_t` | The event that triggers the action. |
| `action` | `gobj_action_fn` | The function to be executed when the event occurs in the specified state. |
| `next_state` | `gobj_state_t` | The state to transition to after executing the action. If `NULL`, the state remains unchanged. |

**Returns**

Returns `0` on success, or `-1` if an error occurs (e.g., if the state does not exist or the event is already defined).

**Notes**

This function ensures that an event-action pair is uniquely associated with a state. If the event is already defined in the state, an error is logged and the function returns `-1`.

---

(gclass_add_event_type)=
## `gclass_add_event_type()`

Adds a new event type to the specified `gclass`. The event type is appended to the list of events associated with the `gclass`.

```C
int gclass_add_event_type(
    hgclass gclass,
    gobj_event_t event_name,
    event_flag_t event_flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The `gclass` to which the event type will be added. |
| `event_type` | `event_type_t *` | Pointer to the `event_type_t` structure defining the event to be added. |

**Returns**

Returns 0 on success, or -1 if an error occurs.

**Notes**

Ensure that the `event_type` does not already exist in the `gclass` before calling [`gclass_add_event_type()`](<#gclass_add_event_type>).

---

(gclass_add_state)=
## `gclass_add_state()`

Adds a new state to the finite state machine of the specified `hgclass`. The state is identified by `state_name` and is used to define event transitions.

```C
int gclass_add_state(
    hgclass         gclass,
    gobj_state_t    state_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The handle to the gclass where the state will be added. |
| `state_name` | `gobj_state_t` | The name of the state to be added. |

**Returns**

Returns `0` on success, or `-1` if an error occurs (e.g., `gclass` is NULL or memory allocation fails).

**Notes**

This function is used to define states in a gclass's finite state machine. Each state can later be associated with event-action pairs using [`gclass_add_ev_action()`](#gclass_add_ev_action).

---

(gclass_attr_desc)=
## `gclass_attr_desc()`

Retrieves the attribute description of a given `gclass`. If `attr` is NULL, it returns the full attribute table.

```C
const sdata_desc_t *gclass_attr_desc(
    hgclass gclass, 
    const char *attr, 
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The `gclass` whose attribute description is to be retrieved. |
| `attr` | `const char *` | The name of the attribute to retrieve. If NULL, returns the full attribute table. |
| `verbose` | `BOOL` | If TRUE, logs an error message when the attribute is not found. |

**Returns**

A pointer to the `sdata_desc_t` structure describing the attribute, or NULL if the attribute is not found.

**Notes**

If `verbose` is set to TRUE and the attribute is not found, an error message is logged.

---

(gclass_authz_desc)=
## `gclass_authz_desc()`

Retrieves the authorization descriptor table for a given `gclass`. The descriptor table defines the access control list (ACL) for the `gclass`.

```C
const sdata_desc_t *gclass_authz_desc(
    hgclass gclass
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The `gclass` whose authorization descriptor table is to be retrieved. |

**Returns**

A pointer to the `sdata_desc_t` structure representing the authorization descriptor table of the `gclass`. Returns `NULL` if the `gclass` is invalid.

**Notes**

This function is useful for inspecting the access control definitions of a `gclass`.

---

(gclass_check_fsm)=
## `gclass_check_fsm()`

Checks the finite state machine (FSM) of a given `hgclass` for consistency, ensuring that all states and events are properly defined.

```C
int gclass_check_fsm(
    hgclass gclass
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The `hgclass` instance whose FSM is to be checked. |

**Returns**

Returns 0 if the FSM is valid, or a negative value if inconsistencies are found.

**Notes**

This function verifies that all states exist, that events are properly defined, and that transitions are valid within the FSM.

---

(gclass_command_desc)=
## `gclass_command_desc()`

Retrieves the data description of a command in a given `gclass`. If `name` is NULL, it returns the full command table.

```C
const sdata_desc_t *gclass_command_desc(
    hgclass gclass,
    const char *name,
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The `gclass` whose command description is to be retrieved. |
| `name` | `const char *` | The name of the command to retrieve. If NULL, returns the full command table. |
| `verbose` | `BOOL` | If TRUE, logs an error message when the command is not found. |

**Returns**

A pointer to the `sdata_desc_t` structure describing the command, or NULL if the command is not found.

**Notes**

If `verbose` is TRUE and the command is not found, an error message is logged.

---

(gclass_create)=
## `gclass_create()`

Creates and registers a new `gclass`, defining its event types, states, methods, and attributes.

```C
hgclass gclass_create(
    gclass_name_t         gclass_name,
    event_type_t         *event_types,
    states_t             *states,
    const GMETHODS      *gmt,
    const LMETHOD       *lmt,
    const sdata_desc_t  *attrs_table,
    size_t               priv_size,
    const sdata_desc_t  *authz_table,
    const sdata_desc_t  *command_table,
    const trace_level_t *s_user_trace_level,
    gclass_flag_t        gclass_flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass_name` | [`gclass_name_t`](#gclass_name_t) | The name of the `gclass` to be created. |
| `event_types` | [`event_type_t *`](#event_type_t) | A list of event types associated with the `gclass`. |
| `states` | [`states_t *`](#states_t) | A list of states defining the finite state machine of the `gclass`. |
| `gmt` | [`const GMETHODS *`](#GMETHODS) | A pointer to the gclass methods table defining the behavior of the `gclass`. |
| `lmt` | [`const LMETHOD *`](#LMETHOD) | A pointer to the local methods table for internal method handling. |
| `attrs_table` | [`const sdata_desc_t *`](#sdata_desc_t) | A pointer to the attribute description table defining the attributes of the `gclass`. |
| `priv_size` | `size_t` | The size of the private data structure allocated for each instance of the `gclass`. |
| `authz_table` | [`const sdata_desc_t *`](#sdata_desc_t) | A pointer to the authorization table defining access control rules. |
| `command_table` | [`const sdata_desc_t *`](#sdata_desc_t) | A pointer to the command table defining available commands for the `gclass`. |
| `s_user_trace_level` | [`const trace_level_t *`](#trace_level_t) | A pointer to the trace level table defining user-defined trace levels. |
| `gclass_flag` | [`gclass_flag_t`](#gclass_flag_t) | Flags defining special properties of the `gclass`. |

**Returns**

Returns a handle to the newly created `gclass`, or `NULL` if an error occurs.

**Notes**

The `gclass_name` must be unique and cannot contain the characters `.` or `^`.
If the `gclass` already exists, an error is logged and `NULL` is returned.
The function initializes the finite state machine (FSM) and event list for the `gclass`.
If the FSM is invalid, the `gclass` is unregistered and `NULL` is returned.

---

(gclass_find_by_name)=
## `gclass_find_by_name()`

Searches for a `gclass` by its name and returns a handle to it if found.

```C
hgclass gclass_find_by_name(
    gclass_name_t gclass_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass_name` | `gclass_name_t` | The name of the `gclass` to search for. |

**Returns**

A handle to the `gclass` if found, otherwise `NULL`.

**Notes**

If the `gclass` is not found, the function returns `NULL` without logging an error.

---

(gclass_event_type)=
## `gclass_event_type()`

Searches for an event in the event list of a given `gclass` and returns its event type if found.

```C
event_type_t *gclass_event_type(
    hgclass gclass,
    gobj_event_t event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The `gclass` in which to search for the event. |
| `event` | `gobj_event_t` | The event to search for in the `gclass` event list. |

**Returns**

A pointer to the `event_type_t` structure if the event is found, otherwise `NULL`.

**Notes**

This function does not check system events. It only searches within the event list of the specified `gclass`.

---

(gclass_find_public_event)=
## `gclass_find_public_event()`

Finds a public event by name in any registered `gclass`. Returns a pointer to the event if found, otherwise returns `NULL`.

```C
gobj_event_t gclass_find_public_event(
    const char *event,
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `event` | `const char *` | The name of the event to search for. |
| `verbose` | `BOOL` | If `TRUE`, logs an error message when the event is not found. |

**Returns**

A pointer to the event if found, otherwise `NULL`.

**Notes**

This function iterates over all registered `gclass` instances to locate a public event. If `verbose` is `TRUE`, an error message is logged when the event is not found.

---

(gclass_gclass_name)=
## `gclass_gclass_name()`

Returns the name of the given `hgclass` as a string.

```C
gclass_name_t gclass_gclass_name(hgclass gclass);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | Handle to the `gclass` whose name is to be retrieved. |

**Returns**

Returns a `gclass_name_t` (const char *) representing the name of the given `gclass`. If `gclass` is NULL, the behavior is undefined.

**Notes**

This function does not allocate memory; the returned string is managed internally.

---

(gclass_gclass_register)=
## `gclass_gclass_register()`

Returns a JSON array containing the registered GClasses with their instance counts.

```C
json_t *gclass_gclass_register(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A JSON array where each element is an object containing the GClass name and the number of instances.

**Notes**

This function is useful for debugging and monitoring the registered GClasses in the system.

---

(gclass_has_attr)=
## `gclass_has_attr()`

Checks if the given `gclass` contains an attribute with the specified `name`.

```C
BOOL gclass_has_attr(
    hgclass gclass,
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The gclass to check for the attribute. |
| `name` | `const char *` | The name of the attribute to search for. |

**Returns**

Returns `TRUE` if the attribute exists in the `gclass`, otherwise returns `FALSE`.

**Notes**

This function performs a case-sensitive search for the attribute name.

---

(gclass_unregister)=
## `gclass_unregister()`

Unregisters a `gclass`, freeing its allocated resources if no instances exist.

```C
void gclass_unregister(
    hgclass hgclass
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | Handle to the `gclass` to be unregistered. |

**Returns**

None.

**Notes**

If instances of the `gclass` still exist, an error is logged and the `gclass` is not unregistered.

---
