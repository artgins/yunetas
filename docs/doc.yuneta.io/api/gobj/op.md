# Operations

Lifecycle and runtime operations on gobj instances: start, stop, play, pause, enable, disable, and related checks.

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)

(gobj_bottom_gobj)=
## `gobj_bottom_gobj()`

Returns the next bottom `gobj` of the given `gobj`. This function is useful for navigating hierarchical `gobj` structures where attributes may be inherited from a lower-level `gobj`.

```C
hgobj gobj_bottom_gobj(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `gobj` whose bottom `gobj` is to be retrieved. |

**Returns**

Returns the next bottom `gobj` of the given `gobj`. If no bottom `gobj` exists, returns `NULL`.

**Notes**

This function is typically used in hierarchical `gobj` structures where attributes and behaviors can be inherited from a lower-level `gobj`.

---

(gobj_change_parent)=
## `gobj_change_parent()`

Changes the parent of the given `hgobj` instance to a new parent `hgobj`. The function updates the internal hierarchy of the object tree.

```C
int gobj_change_parent(
    hgobj gobj,
    hgobj gobj_new_parent
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose parent is to be changed. |
| `gobj_new_parent` | `hgobj` | The new parent `hgobj` instance. |

**Returns**

Returns `0` on success, or `-1` if an error occurs (e.g., if `gobj` or `gobj_new_parent` is `NULL`).

**Notes**

This function ensures that the object hierarchy remains consistent when changing parents. It is useful for dynamically restructuring the object tree.

---

(gobj_child_by_index)=
## `gobj_child_by_index()`

`gobj_child_by_index()` returns the child of a given `hgobj` at the specified index, where the index is relative to 1.

```C
hgobj gobj_child_by_index(
    hgobj gobj,
    size_t index
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent `hgobj` whose child is to be retrieved. |
| `idx` | `size_t` | The index of the child to retrieve, starting from 1. |

**Returns**

Returns the child `hgobj` at the specified index, or `NULL` if the index is out of bounds.

**Notes**

If `idx` is greater than the number of children, the function returns `NULL`.

---

(gobj_child_by_name)=
## `gobj_child_by_name()`

Retrieves the first child of a given `hgobj` that matches the specified name.

```C
hgobj gobj_child_by_name(
    hgobj gobj,
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent `hgobj` whose children will be searched. |
| `name` | `const char *` | The name of the child `hgobj` to find. |

**Returns**

Returns the first child `hgobj` that matches the given name, or `NULL` if no match is found.

**Notes**

This function performs a case-sensitive comparison of the child names.

---

(gobj_child_size)=
## `gobj_child_size()`

Returns the number of child objects directly associated with the given `hgobj`.

```C
size_t gobj_child_size(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_` | `hgobj` | The parent object whose child count is to be retrieved. |

**Returns**

The number of direct child objects of the given `hgobj`.

**Notes**

This function does not count nested children beyond the first level.

---

(gobj_child_size2)=
## `gobj_child_size2()`

Returns the number of child objects of the given `hgobj` that match the specified filter criteria.

```C
size_t gobj_child_size2(
    hgobj gobj_,
    json_t *jn_filter // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_` | `hgobj` | The parent `hgobj` whose children are to be counted. |
| `jn_filter` | `json_t *` | A JSON object containing filter criteria. Only children matching these criteria are counted. |

**Returns**

The number of child objects that match the filter criteria.

**Notes**

The function iterates over the children of [`gobj_`](#gobj_child_size2) and applies the filter conditions specified in `jn_filter`.

---

(gobj_command)=
## `gobj_command()`

Executes a command on the given `hgobj` instance, using either the local command parser or the global command parser if available.

```C
json_t *gobj_command(
    hgobj gobj,
    const char *command,
    json_t *kw,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The target `hgobj` instance on which the command is executed. |
| `command` | `const char *` | The name of the command to execute. |
| `kw` | `json_t *` | A JSON object containing the command parameters. Ownership is transferred to the function. |
| `src` | `hgobj` | The source `hgobj` instance that initiated the command. |

**Returns**

A JSON object containing the command response. The response follows the format `{ "result": int, "comment": string, "schema": json_t *, "data": json_t * }`. The caller is responsible for managing the returned JSON object.

**Notes**

If the target `hgobj` has a local command parser (`mt_command_parser`), it is used. Otherwise, the global command parser is invoked if available. If neither is present, an error response is returned.

---

(gobj_default_service)=
## `gobj_default_service()`

Returns the default service gobj, which is the primary service object in the system.

```C
hgobj gobj_default_service(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A handle to the default service gobj, or NULL if no default service is set.

**Notes**

The default service is typically set using [`gobj_create_default_service()`](<#gobj_create_default_service>).

---

(gobj_disable)=
## `gobj_disable()`

Disables the given `hgobj` instance, preventing it from running or playing. If the object has a `mt_disable` method, it is executed; otherwise, [`gobj_stop_tree()`](#gobj_stop_tree) is called.

```C
int gobj_disable(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance to disable. |

**Returns**

Returns 0 on success, or -1 if the object is already disabled or an error occurs.

**Notes**

If the object is already disabled, a warning is logged, and no action is taken.

---

(gobj_enable)=
## `gobj_enable()`

Enables the specified `hgobj` by setting its disabled flag to `FALSE` and starting its execution if necessary. If the object has a custom `mt_enable` method, it is invoked; otherwise, [`gobj_start_tree()`](#gobj_start_tree) is called.

```C
int gobj_enable(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The object to be enabled. |

**Returns**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

If the object is already enabled, this function has no effect.

---

(gobj_find_child)=
## `gobj_find_child()`

Finds the first child of `gobj` that matches the given filter criteria.

```C
hgobj gobj_find_child(
    hgobj gobj,
    json_t *jn_filter // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent `gobj` whose children will be searched. |
| `jn_filter` | `json_t *` | A JSON object containing filter criteria. The first child that matches these criteria is returned. |

**Returns**

Returns the first child `gobj` that matches the filter criteria, or `NULL` if no match is found.

**Notes**

The function iterates over the direct children of `gobj` and applies the filter criteria to find a match.

---

(gobj_find_gobj)=
## `gobj_find_gobj()`

Finds a `gobj` by its path within the hierarchical structure of `gobj` instances.

```C
hgobj gobj_find_gobj(
    const char *gobj_path
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | The hierarchical path of the `gobj` to find, using '`' as a separator. |

**Returns**

Returns a handle to the `gobj` if found, otherwise returns `NULL`.

**Notes**

If `path` is `__default_service__`, it returns the default service `gobj`. If `path` is `__yuno__` or `__root__`, it returns the root `gobj`.

---

(gobj_find_service)=
## `gobj_find_service()`

Searches for a service gobj by its name, performing a case-insensitive comparison.

```C
hgobj gobj_find_service(
    const char *service,
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `service` | `const char *` | The name of the service to search for. The comparison is case-insensitive. |
| `verbose` | `BOOL` | If set to `TRUE`, logs an error message when the service is not found. |

**Returns**

Returns a handle to the found service gobj, or `NULL` if the service is not found.

**Notes**

If `service` is `__default_service__`, the function returns the default service gobj. If `service` is `__yuno__` or `__root__`, it returns the Yuno gobj.

---

(gobj_find_service_by_gclass)=
## `gobj_find_service_by_gclass()`

Finds a service gobj by its gclass name. Returns a handle to the first matching service or NULL if not found.

```C
hgobj gobj_find_service_by_gclass(
    const char *gclass_name,
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass_name` | `const char *` | The name of the gclass to search for. |
| `verbose` | `BOOL` | If TRUE, logs an error message when the service is not found. |

**Returns**

Returns a handle to the first service gobj matching the given gclass name, or NULL if no match is found.

**Notes**

If `verbose` is set to TRUE and no matching service is found, an error message is logged.

---

(gobj_first_child)=
## `gobj_first_child()`

Returns the first child of the given `hgobj` object.

```C
hgobj gobj_first_child(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent `hgobj` whose first child is to be retrieved. |

**Returns**

Returns the first child `hgobj` of the given parent. If the parent has no children, returns `NULL`.

**Notes**

This function does not modify the object hierarchy; it only retrieves the first child.

---

(gobj_free_iter)=
## `gobj_free_iter()`

Decrements the reference count of each gobj in the given JSON array and frees the array.

```C
int gobj_free_iter(
    json_t *iter
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `iter` | `json_t *` | A JSON array containing references to gobj instances. |

**Returns**

None.

**Notes**

Each gobj in the array has its reference count decremented before the array itself is freed.

---

(gobj_last_bottom_gobj)=
## `gobj_last_bottom_gobj()`

Returns the last bottom `gobj` in the hierarchy of the given `gobj`. This function traverses the bottom `gobj` chain to find the deepest `gobj` in the hierarchy.

```C
hgobj gobj_last_bottom_gobj(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `gobj` whose last bottom `gobj` is to be retrieved. |

**Returns**

Returns the last bottom `gobj` in the hierarchy if it exists, otherwise returns `NULL`.

**Notes**

This function is useful when dealing with a stack of `gobj` instances that act as a unit, ensuring that operations are performed on the deepest `gobj` in the hierarchy.

---

(gobj_last_child)=
## `gobj_last_child()`

Returns the last child of the given `hgobj` object.

```C
hgobj gobj_last_child(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent `hgobj` whose last child is to be retrieved. |

**Returns**

Returns the last child `hgobj` of the given parent. If the parent has no children, returns `NULL`.

**Notes**

This function is useful for iterating over child objects in reverse order.

---

(gobj_local_method)=
## `gobj_local_method()`

Executes a local method (`lmethod`) on the given `hgobj` instance, passing the provided keyword arguments (`kw`) and source object (`src`).

```C
json_t *gobj_local_method(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance on which the local method is executed. |
| `lmethod` | `const char *` | The name of the local method to execute. |
| `kw` | `json_t *` | A JSON object containing the keyword arguments for the method. The ownership of this parameter is transferred to the function. |
| `src` | `hgobj` | The source `hgobj` instance invoking the method. |

**Returns**

Returns a JSON object containing the result of the executed method, or `NULL` if the method is not found or an error occurs.

**Notes**

If the specified `lmethod` is not found in the local method table (`lmt`), an error is logged, and `NULL` is returned.

---

(gobj_match_children)=
## `gobj_match_children()`

Returns a list of child objects that match the specified filter criteria.

```C
json_t *gobj_match_children(
    hgobj gobj,
    json_t *jn_filter   // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent object whose children will be searched. |
| `jn_filter` | `json_t *` | A JSON object containing filter criteria. The function takes ownership of this parameter. |

**Returns**

A JSON array containing matching child objects. The caller is responsible for freeing the returned JSON object.

**Notes**

Use [`gobj_free_iter()`](#gobj_free_iter) to free the returned JSON array when it is no longer needed.

---

(gobj_match_children_tree)=
## `gobj_match_children_tree()`

Returns an iterator (JSON list of `hgobj`) containing all matched child objects, including deep levels of children. It traverses the entire tree of child objects and applies the given filter.

```C
json_t *gobj_match_children_tree(
    hgobj gobj,
    json_t *jn_filter   // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent object whose child tree will be searched. |
| `jn_filter` | `json_t *` | A JSON object containing filter criteria. Only child objects matching this filter will be included in the result. |

**Returns**

A JSON array containing matched child objects. The returned list must be freed using `gobj_free_iter()`.

**Notes**

This function performs a deep search, checking all levels of child objects. The filter is applied recursively to all descendants.

---

(gobj_match_gobj)=
## `gobj_match_gobj()`

Checks if a given `hgobj` matches the specified filter criteria in `jn_filter`. The function evaluates attributes and system-defined keys to determine if the object meets the conditions.

```C
BOOL gobj_match_gobj(
    hgobj gobj,
    json_t *jn_filter // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance to be checked against the filter. |
| `jn_filter` | `json_t *` | A JSON object containing filter conditions. System-defined keys such as `__gclass_name__`, `__gobj_name__`, and `__state__` can be used for filtering. |

**Returns**

Returns `TRUE` if the `hgobj` matches the filter criteria, otherwise returns `FALSE`.

**Notes**

The function supports filtering based on system-defined keys like `__gclass_name__`, `__gobj_name__`, `__state__`, and `__disabled__`. It also compares attribute values using simple JSON comparison.

---

(gobj_next_child)=
## `gobj_next_child()`

Returns the next sibling of the given gobj in the parent's child list.

```C
hgobj gobj_next_child(
    hgobj child
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `child` | `hgobj` | The gobj whose next sibling is to be retrieved. |

**Returns**

Returns the next sibling gobj if available, otherwise returns NULL.

**Notes**

If `child` is NULL, an error is logged.

---

(gobj_pause)=
## `gobj_pause()`

The `gobj_pause()` function pauses the execution of a given `hgobj` instance if it is currently playing. If the object is not playing, a warning is logged.

```C
int gobj_pause(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance to be paused. |

**Returns**

Returns 0 on success. If the object is not playing, a warning is logged and -1 is returned.

**Notes**

If the object is already paused, a warning is logged. If the object has a `mt_pause` method, it is called.

---

(gobj_play)=
## `gobj_play()`

The `gobj_play()` function transitions a GObj into the playing state, invoking its `mt_play` method if defined. If the GObj is not already running, it will be started unless the `gcflag_required_start_to_play` flag is set.

```C
int gobj_play(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Handle to the GObj instance to be played. |

**Returns**

Returns 0 on success, or a negative value if an error occurs.

**Notes**

['If the GObj is already playing, a warning is logged.', 'If the GObj is disabled, it cannot be played.', 'If the GObj is not running, it will be started unless `gcflag_required_start_to_play` is set.', "If the GObj's `mt_play` method is defined, it will be invoked."]

---

(gobj_prev_child)=
## `gobj_prev_child()`

Returns the previous sibling of the given gobj in its parent's child list.

```C
hgobj gobj_prev_child(
    hgobj child
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `child` | `hgobj` | The gobj whose previous sibling is to be retrieved. |

**Returns**

Returns the previous sibling gobj if it exists, otherwise returns NULL.

**Notes**

If the given `child` is the first child in the parent's list, the function returns NULL.

---

(gobj_search_path)=
## `gobj_search_path()`

Searches for a `hgobj` instance by traversing the object tree using a path with '`' as a separator.

```C
hgobj gobj_search_path(
    hgobj gobj,
    const char *path
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_` | `hgobj` | The root object from which the search begins. |
| `path_` | `const char *` | The path to the target object, using '`' as a separator between hierarchy levels. |

**Returns**

Returns the `hgobj` instance found at the specified path, or `NULL` if no matching object is found.

**Notes**

The function supports searching by both gclass and gobj names. If a segment contains a '^', it is treated as 'gclass^gobj_name'.

---

(gobj_services)=
## `gobj_services()`

Returns a JSON array containing the names of all registered services in the system.

```C
json_t *gobj_services(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A JSON array of strings, where each string represents the name of a registered service. The caller is responsible for freeing the returned JSON object.

**Notes**

This function provides a list of services that have been registered using [`gobj_create_service()`](<#gobj_create_service>) or similar functions.

---

(gobj_set_bottom_gobj)=
## `gobj_set_bottom_gobj()`

Sets the bottom gobj of a given gobj, allowing attribute inheritance from the specified bottom gobj.

```C
hgobj gobj_set_bottom_gobj(
    hgobj gobj,
    hgobj bottom_gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The gobj whose bottom gobj is being set. |
| `bottom_gobj` | `hgobj` | The gobj to be set as the bottom gobj. |

**Returns**

Returns 0 on success. If the bottom gobj is already set, a warning is logged, but the function does not override the existing bottom gobj.

**Notes**

The bottom gobj is used for attribute inheritance. If a gobj already has a bottom gobj, setting a new one will not override the existing one.

---

(gobj_start)=
## `gobj_start()`

The `gobj_start()` function starts the specified `hgobj` instance, transitioning it to a running state if it is not already running. It verifies required attributes before starting and invokes the `mt_start` method of the associated gclass if defined.

```C
int gobj_start(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance to be started. |

**Returns**

Returns 0 on success, or -1 if the gobj is already running, disabled, or missing required attributes.

**Notes**

If the gobj is already running, an error is logged and the function returns -1.
If the gobj is disabled, it cannot be started.
Before starting, the function checks for required attributes and logs an error if any are missing.
If the gclass has an `mt_start` method, it is invoked to perform additional start operations.

---

(gobj_start_children)=
## `gobj_start_children()`

Starts all direct child objects of the given `hgobj` instance by invoking [`gobj_start()`](#gobj_start) on each child that is not already running and not disabled.

```C
int gobj_start_children(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent object whose direct child objects will be started. |

**Returns**

Returns 0 on success.

**Notes**

This function only starts the direct children of [`gobj`](#gobj). It does not recursively start the entire tree of child objects. To start all descendants, use [`gobj_start_tree()`](#gobj_start_tree).

---

(gobj_start_tree)=
## `gobj_start_tree()`

Starts the given `gobj` and all its child objects recursively, unless they have the `gcflag_manual_start` flag set.

```C
int gobj_start_tree(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The root object whose tree of child objects will be started. |

**Returns**

Returns 0 on success, or a negative value if an error occurs.

**Notes**

If a child object has the `gcflag_manual_start` flag set, it will not be started automatically.

---

(gobj_stats)=
## `gobj_stats()`

Retrieves statistics for the given `hgobj`. If the gclass has a `mt_stats` method, it is used; otherwise, the global statistics parser is invoked.

```C
json_t *gobj_stats(
    hgobj gobj,
    const char *stats,
    json_t *kw,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose statistics are to be retrieved. |
| `stats` | `const char *` | The name of the statistics to retrieve. If NULL, all available statistics are returned. |
| `kw` | `json_t *` | Additional parameters for filtering or modifying the statistics retrieval. Owned by the function. |
| `src` | `hgobj` | The source `hgobj` requesting the statistics. |

**Returns**

A JSON object containing the requested statistics. The caller is responsible for managing the returned JSON object.

**Notes**

If the gclass has a `mt_stats` method, it is used to retrieve the statistics. Otherwise, the global statistics parser is invoked. If neither is available, an error response is returned.

---

(gobj_stop)=
## `gobj_stop()`

Stops the execution of the given `gobj` instance, ensuring it is no longer running. If the `gobj` is playing, it will be paused before stopping.

```C
int gobj_stop(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `gobj` instance to be stopped. |

**Returns**

Returns 0 on success, or -1 if the `gobj` is already stopped, destroying, or invalid.

**Notes**

If the `gobj` is playing, it will be paused before stopping. If the `gobj` is already stopped, an error is logged.

---

(gobj_stop_children)=
## `gobj_stop_children()`

Stops all direct child objects of the given `hgobj` instance by invoking [`gobj_stop()`](#gobj_stop) on each child.

```C
int gobj_stop_children(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent `hgobj` whose direct child objects will be stopped. |

**Returns**

Returns 0 on success, or -1 if an error occurs (e.g., if `gobj` is `NULL` or being destroyed).

**Notes**

This function only stops the direct children of `gobj`, not the entire hierarchy. To stop all descendants, use [`gobj_stop_tree()`](#gobj_stop_tree).

---

(gobj_stop_tree)=
## `gobj_stop_tree()`

Stops the given `gobj` and all its child objects recursively. If the `gobj` is already being destroyed, it logs an error and returns immediately.

```C
int gobj_stop_tree(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The root object whose tree of child objects will be stopped. |

**Returns**

Returns 0 on success, or -1 if an error occurs (e.g., `gobj` is NULL or being destroyed).

**Notes**

If the `gobj` is already being destroyed, an error is logged and the function returns -1.
This function first stops the given `gobj` and then recursively stops all its child objects.
Uses [`gobj_walk_gobj_children_tree()`](#gobj_walk_gobj_children_tree) to traverse the child objects.

---

(gobj_walk_gobj_children)=
## `gobj_walk_gobj_children()`

Traverses the direct child objects of the given `hgobj` using the specified traversal method and applies a callback function to each child.

```C
int gobj_walk_gobj_children(
    hgobj gobj,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2,
    void *user_data3
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent `hgobj` whose direct children will be traversed. |
| `walk_type` | `walk_type_t` | The traversal method, which determines the order in which child objects are visited. |
| `cb_walking` | `cb_walking_t` | A callback function that is applied to each child `hgobj`. |
| `user_data` | `void *` | User-defined data passed to the callback function. |
| `user_data2` | `void *` | Additional user-defined data passed to the callback function. |

**Returns**

Returns 0 on success, or a negative value if an error occurs.

**Notes**

This function only traverses the direct children of the given `hgobj`. To traverse the entire hierarchy, use [`gobj_walk_gobj_children_tree()`](#gobj_walk_gobj_children_tree).

---

(gobj_walk_gobj_children_tree)=
## `gobj_walk_gobj_children_tree()`

Traverses the child objects of a given `hgobj` in a specified order and applies a callback function to each child.

```C
int gobj_walk_gobj_children_tree(
    hgobj gobj,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2,
    void *user_data3
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent `hgobj` whose child objects will be traversed. |
| `walk_type` | `walk_type_t` | Specifies the traversal order, such as `WALK_TOP2BOTTOM`, `WALK_BOTTOM2TOP`, or `WALK_BYLEVEL`. |
| `cb_walking` | `cb_walking_t` | A callback function that is applied to each child `hgobj`. |
| `user_data` | `void *` | User-defined data passed to the callback function. |
| `user_data2` | `void *` | Additional user-defined data passed to the callback function. |

**Returns**

Returns 0 on success or a negative value if an error occurs.

**Notes**

The callback function should return 0 to continue traversal, a negative value to stop traversal, or a positive value to skip the current branch when using `WALK_TOP2BOTTOM`.

---

(gobj_find_child_by_tree)=
## `gobj_find_child_by_tree()`

Searches the entire gobj subtree (recursively) for the first child that matches the given filter conditions. This is the recursive counterpart to `gobj_find_child()`, which only searches first-level children.

```C
hgobj gobj_find_child_by_tree(
    hgobj gobj,
    json_t *jn_filter
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The root gobj whose subtree will be searched. |
| `jn_filter` | `json_t *` | A JSON object with filter conditions (e.g., `{"__gclass_name__": "...", "__gobj_name__": "..."}`). Ownership is transferred to the function. |

**Returns**

A handle to the first matching child gobj, or `NULL` if no match is found.

**Notes**

The filter uses the same matching logic as `gobj_match_gobj()`. The search traverses the tree depth-first and returns the first match.

---

