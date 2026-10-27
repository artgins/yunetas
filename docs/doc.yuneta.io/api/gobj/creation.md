# Creation

Create, destroy, and navigate gobj instances. Every gobj belongs to a parent and a GClass, and lives inside the hierarchical tree that forms a yuno.

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/7.28.8/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/7.28.8/kernel/c/gobj-c/src/gobj.c)

(gobj_create)=
## [`gobj_create()`](https://github.com/artgins/yunetas/blob/7.28.8/kernel/c/gobj-c/src/gobj.c#L2185)

Creates a new `gobj` (generic object) instance of the specified `gclass` and assigns it to a parent `gobj` if provided.

```C
hgobj gobj_create(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw, // owned
    hgobj           parent
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_name` | `const char *` | The name of the `gobj` to be created. It is case-insensitive and converted to lowercase. |
| `gclass_name` | `gclass_name_t` | The name of the `gclass` to which the `gobj` belongs. |
| `kw` | `json_t *` | A JSON object containing configuration parameters for the `gobj`. The ownership of this object is transferred to the function. |
| `parent` | `hgobj` | The parent `gobj` to which the new `gobj` will be attached. If `NULL`, the `gobj` is created without a parent. |

**Returns**

Returns a handle to the newly created `gobj` (`hgobj`). Returns `NULL` if the creation fails due to invalid parameters or memory allocation failure.

**Notes**

['The function internally calls [`gobj_create2()`](#gobj_create2) with default flags.', 'If `gobj_name` is longer than 15 characters on ESP32, the function will abort execution.', 'If `gclass_name` is empty or not found, an error is logged and `NULL` is returned.', "The function makes sure that `gobj_name` does not contain invalid characters such as '`' or '^'.", 'If `parent` is `NULL`, an error is logged unless the `gobj` is a Yuno instance.']

---

(gobj_create2)=
## [`gobj_create2()`](https://github.com/artgins/yunetas/blob/7.28.8/kernel/c/gobj-c/src/gobj.c#L1818)

Creates a new `gobj` (generic object) with the specified name, class, attributes, and parent. The function initializes the object, sets its attributes, and registers it if it is a service.

```C
hgobj gobj_create2(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw,    // owned
    hgobj           parent,
    gobj_flag_t     gobj_flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_name` | `const char *` | The name of the `gobj` to be created. It is case-insensitive and converted to lowercase. |
| `gclass_name` | [`gclass_name_t`](#gclass_name_t) | The name of the `gclass` to which the `gobj` belongs. |
| `kw` | `json_t *` | A JSON object containing the attributes and configuration for the `gobj`. The function takes ownership of this parameter. |
| `parent` | `hgobj` | The parent `gobj` under which the new `gobj` will be created. Must be non-null unless the `gobj` is a Yuno. |
| `gobj_flag` | [`gobj_flag_t`](#gobj_flag_t) | Flags that define the behavior of the `gobj`, such as whether it is a service, volatile, or a pure child. |

**Returns**

Returns a handle to the newly created `gobj` (`hgobj`). 
Returns `NULL` if creation fails due to invalid parameters or memory allocation failure.

**Notes**

If the `gobj` is marked as a service, it is registered globally.
If the `gobj` is a Yuno, it is stored as the global Yuno instance.

The function checks for required attributes and applies global configuration variables.
If the `gobj` has a [`mt_create2`](#mt_create2) method, it is called with the provided attributes.

If the `gobj` has a parent, it is added to the parent's child list. A parent that is being destroyed is refused, with an ERROR (*"Cannot create a child of a gobj that is being destroyed"*), and the answer is `NULL` (7.27.0): its children are being destroyed or are gone, so the new one would be freed with it, or never destroyed. It is what a child's `mt_stop` or `mt_destroy` meets when it creates a sibling while its parent is destroyed:

```C
PRIVATE int mt_stop(hgobj gobj)
{
    hgobj next = gobj_create("next", C_WORKER, 0, gobj_parent(gobj));
    if(!next) {
        return 0;   // Error already logged: the parent is going too
    }
    gobj_start(next);
    return 0;
}
```

---

(gobj_create_default_service)=
## [`gobj_create_default_service()`](https://github.com/artgins/yunetas/blob/7.28.8/kernel/c/gobj-c/src/gobj.c#L2140)

The function `gobj_create_default_service()` creates a default service gobj with autostart enabled but autoplay disabled. The service will be played by the yuno's play method.

```C
hgobj gobj_create_default_service(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw, // owned
    hgobj           parent
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_name` | `const char *` | The name of the gobj to be created. |
| `gclass_name` | `gclass_name_t` | The name of the gclass to which the gobj belongs. |
| `kw` | `json_t *` | A JSON object containing the configuration parameters for the gobj. The ownership of this object is transferred to the function. |
| `parent` | `hgobj` | The parent gobj under which the new gobj will be created. |

**Returns**

Returns a handle to the newly created gobj, or NULL if the creation fails.

**Notes**

The created gobj is marked as a default service and will be started automatically but not played until explicitly triggered by the yuno's play method.

---

(gobj_create_pure_child)=
## [`gobj_create_pure_child()`](https://github.com/artgins/yunetas/blob/7.28.8/kernel/c/gobj-c/src/gobj.c#L2170)

Creates a new `gobj` as a pure child of the specified parent. A pure child sends events directly to its parent instead of publishing them.

```C
hgobj gobj_create_pure_child(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw,    // owned
    hgobj           parent
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_name` | `const char *` | The name of the new `gobj`. |
| `gclass_name` | `gclass_name_t` | The `gclass` type of the new `gobj`. |
| `kw` | `json_t *` | A JSON object containing the configuration attributes for the new `gobj`. This parameter is owned and will be consumed by the function. |
| `parent` | `hgobj` | The parent `gobj` to which the new `gobj` will be attached. |

**Returns**

Returns a handle to the newly created `gobj`, or `NULL` if creation fails.

**Notes**

A pure child does not publish events but instead sends them directly to its parent. This function internally calls [`gobj_create2()`](#gobj_create2) with the `gobj_flag_pure_child` flag.

---

(gobj_create_service)=
## [`gobj_create_service()`](https://github.com/artgins/yunetas/blob/7.28.8/kernel/c/gobj-c/src/gobj.c#L2131)

The `gobj_create_service()` function creates a new service GObj instance with the specified name, gclass, and configuration, and assigns it to the given parent GObj.

```C
hgobj gobj_create_service(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw,  // owned
    hgobj parent
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_name` | `const char *` | The name of the GObj instance to be created. |
| `gclass_name` | `gclass_name_t` | The name of the GClass to which the new GObj belongs. |
| `kw` | `json_t *` | A JSON object containing the configuration parameters for the new GObj. This parameter is owned and will be consumed by the function. |
| `parent` | `hgobj` | The parent GObj under which the new service GObj will be created. |

**Returns**

Returns a handle to the newly created service GObj (`hgobj`). If the creation fails, it returns `NULL`.

**Notes**

The created GObj is marked as a service using the `gobj_flag_service` flag. If the `gclass_name` is not found, an error is logged, and the function returns `NULL`. The function internally calls [`gobj_create2()`](#gobj_create2) with the appropriate flags.

---

(gobj_create_tree)=
## [`gobj_create_tree()`](https://github.com/artgins/yunetas/blob/7.28.8/kernel/c/gobj-c/src/gobj.c#L2615)

Creates a hierarchical tree of `gobj` instances from a JSON configuration string. The function parses the configuration, applies global settings, and instantiates the objects accordingly.

```C
hgobj gobj_create_tree(
    hgobj parent,
    const char *tree_config,
    json_t *json_config_variables
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `parent` | `hgobj` | The parent `gobj` under which the tree will be created. |
| `tree_config` | `const char *` | A JSON string defining the structure and attributes of the `gobj` tree. |
| `json_config_variables` | `json_t *` | A JSON object (owned) containing configuration variables to be applied to the tree. |

**Returns**

Returns the root `gobj` of the created tree, or `NULL` if an error occurs.

**Notes**

This function internally calls `gobj_create_tree0()` after parsing and applying configuration variables.

---

(gobj_create_volatil)=
## [`gobj_create_volatil()`](https://github.com/artgins/yunetas/blob/7.28.8/kernel/c/gobj-c/src/gobj.c#L2155)

Creates a new volatile `gobj` instance of the specified `gclass_name` with the given attributes and parent. A volatile `gobj` is automatically destroyed when its parent is destroyed.

```C
hgobj gobj_create_volatil(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw, // owned
    hgobj           parent
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_name` | `const char *` | The name of the `gobj` instance. |
| `gclass_name` | `gclass_name_t` | The name of the `gclass` to instantiate. |
| `kw` | `json_t *` | A JSON object containing the attributes for the `gobj`. The ownership of this object is transferred to the function. |
| `parent` | `hgobj` | The parent `gobj` to which the new `gobj` will be attached. |

**Returns**

Returns a handle to the newly created volatile `gobj`, or `NULL` if creation fails.

**Notes**

A volatile `gobj` is automatically destroyed when its parent is destroyed. Use [`gobj_create2()`](#gobj_create2) for more control over `gobj` creation.

---

(gobj_create_yuno)=
## [`gobj_create_yuno()`](https://github.com/artgins/yunetas/blob/7.28.8/kernel/c/gobj-c/src/gobj.c#L2123)

Creates a new Yuno object with the specified name and gclass. The Yuno object serves as the root object in the hierarchical structure of gobjs.

```C
hgobj gobj_create_yuno(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_name` | `const char *` | The name of the Yuno object. |
| `gclass_name` | `gclass_name_t` | The name of the gclass to associate with the Yuno object. |
| `kw` | `json_t *` | A JSON object containing configuration parameters for the Yuno object. The ownership of this parameter is transferred to the function. |

**Returns**

Returns a handle to the newly created Yuno object, or NULL if creation fails.

**Notes**

The Yuno object is the top-level object in the gobj hierarchy and must be unique within the system.

---

(gobj_destroy)=
## [`gobj_destroy()`](https://github.com/artgins/yunetas/blob/7.28.8/kernel/c/gobj-c/src/gobj.c#L2665)

The `gobj_destroy()` function deallocates and removes a given `hgobj` instance. This makes sure of proper cleanup of its resources, subscriptions, and child objects.

```C
void gobj_destroy(
    hgobj   gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the `hgobj` instance to be destroyed. |

**Returns**

This function does not return a value.

**Notes**

- A playing gobj is paused, and a running one stopped, before it is
  destroyed, each with an ERROR: destroying a live gobj is the caller's bug
  (call `gobj_stop_tree()` first).
- That pause and that stop are watched: a stop that publishes `EV_STOPPED` at
  once (a `C_TCP` with nothing in flight) lets its host destroy the gobj in
  there, the whole destroy. Then `gobj_destroy()` returns, with nothing left
  to do and nothing of the gobj read (7.27.0; up to 7.26.9 it went on with
  the freed gobj). The caller must not touch the gobj after `gobj_destroy()`
  in any case.
- Its children are destroyed one by one, watched the same way: when the
  destroy of one child destroys the next (its stop, for a running one), it
  goes on from the first child left after it. No child can be created
  meanwhile: from the moment its children are destroyed, `gobj_create()`
  refuses it as a parent, with an ERROR (7.27.0).
- Its subscriptions, both ways, are removed, a service is deregistered, its
  children are destroyed (before its `mt_destroy()`), and the events it left
  posted are dropped.

**Destroyed inside a dispatch.** A gobj can be destroyed while an event is
dispatched to it or published by it: an action that destroys its own gobj at
the end of its work, a host that destroys a stopped transport in its
`EV_STOPPED`. The kernel keeps a frame for each dispatch in progress
([`gobj_send_event()`](events_state.md#gobj_send_event),
[`gobj_publish_event()`](publish.md#gobj_publish_event)), and
`gobj_destroy()` marks the frames of the gobj it destroys, and of each gobj of
its subtree. After the action, or after each subscriber, the kernel asks the
frame, never the gobj: a publication whose publisher is gone stops, and the
freed gobj is not read. Up to 7.26.9 the kernel read the gobj's `obflag` for
that, in freed memory.

**A gobj must not be freed under code that is still running on it.** The
kernel survives it (the frames), the gclass code does not: it goes on when the
inner call returns, on a gobj that is gone. So when a destroy finds code still
running on the gobj, or on a gobj of its subtree, further up the stack, it
logs an ERROR with a stack, and destroys all the same:

- *"Destroying a gobj while its tree is publishing"*: a subscriber destroyed
  the publisher, or one of its ancestors, inside the publication
  (`C_TIMER` reads its own data right after it publishes `EV_TIMEOUT`);
- *"Destroying a gobj while one of its actions is still running"*: an inner
  dispatch destroyed a gobj that an outer action, or framework method, of it
  is still running on (a `C_TASK` whose result arrived inside the job's
  action, up to 7.26.9).

Both name the gobj destroyed (`destroying`), the gobj still running
(`publisher` or `running`) and its `event` (or framework method), and the
innermost code that destroys (`destroyer`, `inner_event`). Which code counts:
every live frame of the subtree except

- the innermost one: it is the code that destroys, and knows it. A gobj that
  destroys itself in its own action, at the end of its work, with nothing of
  it further up the stack, is legal;
- the `EV_STOPPED` publication of a gobj, and the frames of that gobj it runs
  inside of (the `mt_stop` or the action that published it, an outer
  publication of it). By the stop contract it is the last thing a gobj does
  with itself, its host destroys it there (`C_TCP`, `C_UDP_S`, `C_PTY`), and
  the code it returns to returns at once. NOT a frame of that gobj that began
  inside its `EV_STOPPED` (an action its host sends it from there): it does
  not know, and it is an ERROR;
- the kernel's own watch frames (the walks of the tree, and the pause and
  stop inside `gobj_destroy()`), which run no gclass code.

The fix is to leave the stack: post an event to yourself with
[`gobj_post_event()`](events_state.md#gobj_post_event), and destroy in its
action. `C_TASK` ends so, always, and `C_COUNTER` when it expires (7.27.0):

```C
/*
 *  Wrong: EV_TIMEOUT comes from the C_TIMER child, inside its publication,
 *  and destroying this gobj destroys the timer under it.
 */
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    gobj_stop(gobj);
    gobj_destroy(gobj);     // ERROR: its tree is publishing
    return 0;
}

/*
 *  Right: the end runs on the next turn of the loop, from a clean stack.
 *  EV_FINISH is an input event of the gclass, in its FSM and event_types.
 */
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    gobj_post_event(gobj, EV_FINISH, 0, gobj);
    return 0;
}

PRIVATE int ac_finish(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    gobj_stop(gobj);
    gobj_destroy(gobj);     // its own action, outside any publication: legal
    return 0;
}
```

The host of a transport destroys it in its `EV_STOPPED`, and that is legal:

```C
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);  // the stop contract: EV_STOPPED is src's last act
    }
    KW_DECREF(kw)
    return 0;
}
```

---

(gobj_destroy_children)=
## [`gobj_destroy_children()`](https://github.com/artgins/yunetas/blob/7.28.8/kernel/c/gobj-c/src/gobj.c#L2881)

Destroys all child objects of the given `hgobj`. This makes sure of proper cleanup and deallocation.

```C
void gobj_destroy_children(
    hgobj   gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent object whose child objects will be destroyed. |

**Returns**

This function does not return a value.

**Notes**

Each child object is destroyed using [`gobj_destroy()`](#gobj_destroy). If a child is already destroyed or in the process of being destroyed, an error is logged. The destroy of one child may destroy others (its stop, for a running one): the walk goes on from the first child left after it. It destroys the children `gobj` has when it begins: a child created while it runs (a host that renews its transport in the transport's `EV_STOPPED`) lives on, as `gobj` does (7.27.0).

```C
gobj_destroy_children(host);    // the old children are gone; what host's
                                // EV_STOPPED created meanwhile is left
```

---

(gobj_service_factory)=
## [`gobj_service_factory()`](https://github.com/artgins/yunetas/blob/7.28.8/kernel/c/gobj-c/src/gobj.c#L2474)

Creates a service gobj using `gobj_create_tree0()` and registers it as a service.

```C
hgobj gobj_service_factory(
    const char  *name,
    json_t      *jn_service_config // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `name` | `const char *` | The name of the service to be created. |
| `jn_service_config` | `json_t *` | A JSON object containing the service configuration. This parameter is owned and will be consumed by the function. |

**Returns**

Returns a handle to the created service gobj, or `NULL` if the creation fails.

**Notes**

The function extracts global settings, applies configuration variables, and invokes `gobj_create_tree0()` to instantiate the service.

---

(gobj_destroy_named_children)=
## [`gobj_destroy_named_children()`](https://github.com/artgins/yunetas/blob/7.28.8/kernel/c/gobj-c/src/gobj.c#L2953)

Destroys all direct children of the given gobj that match the specified name. Before destruction, each matching child is automatically paused and stopped if it is currently running or playing. The function walks first-level children only.

```C
int gobj_destroy_named_children(
    hgobj gobj,
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent gobj whose children will be searched. |
| `name` | `const char *` | The name to match against child gobj names. All children with this name are destroyed. |

**Returns**

Returns `0` on success, or `-1` with an ERROR if `gobj` is `NULL`, or `name` is `NULL` or empty (7.27.0; up to 7.26.9 a `NULL` name crashed).

**Notes**

Matching children are automatically paused and stopped before being destroyed. Only first-level children are considered (not recursive).

```C
gobj_destroy_named_children(priv->gobj_toolbar, "clock");    // every child named "clock"
```

---

(gobj_sdata_create)=
## [`gobj_sdata_create()`](https://github.com/artgins/yunetas/blob/7.28.8/kernel/c/gobj-c/src/gobj.c#L3542)

Creates a new JSON object populated with default values according to the given `sdata_desc_t` schema. Each attribute defined in the schema is initialized to its declared default value (string, boolean, integer, real, JSON list, or JSON dict). This is an attribute function without bottom inheritance.

```C
json_t *gobj_sdata_create(
    hgobj gobj,
    const sdata_desc_t* schema
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj context used for logging. |
| `schema` | `const sdata_desc_t *` | A null-terminated array of `sdata_desc_t` descriptors defining attribute names, types, and default values. |

**Returns**

A new JSON object (owned by the caller) with one key per schema attribute, each set to its default value.

**Notes**

This function is called internally during gobj creation to initialize the gobj's attribute dictionary (`jn_attrs`). It can also be used independently to build data records from any `sdata_desc_t` schema (for example subscription records).

---

