# Trace Functions

Source code in:
- [gobj.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)
- [gobj.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)

(gobj_add_trace_filter)=
## `gobj_add_trace_filter()`

Adds a trace filter to a given gclass, allowing selective tracing based on attribute values.

```C
int gobj_add_trace_filter(
    hgclass gclass,
    const char *attr,
    const char *value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The gclass to which the trace filter will be applied. |
| `attr` | `const char *` | The attribute name to filter traces on. |
| `value` | `const char *` | The attribute value that must match for tracing to be enabled. |

**Returns**

Returns 0 on success, or -1 if the attribute is invalid or the gclass does not support the specified attribute.

**Notes**

This function allows filtering trace messages based on specific attribute values. If the attribute does not exist in `hgclass`, an error is logged.

---

(gobj_get_deep_tracing)=
## `gobj_get_deep_tracing()`

Retrieves the current deep tracing level, which determines the verbosity of trace logging.

```C
int gobj_get_deep_tracing(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Returns an integer representing the deep tracing level. A value of 1 enables tracing while considering `__gobj_trace_no_level__`, and values greater than 1 enable full tracing.

**Notes**

This function is useful for debugging and monitoring purposes, allowing developers to control the depth of trace logging.

---

(gobj_get_gclass_trace_level)=
## `gobj_get_gclass_trace_level()`

Retrieves the trace levels set for the specified `hgclass`.

```C
json_t *gobj_get_gclass_trace_level(
    hgclass gclass
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The `hgclass` whose trace levels are to be retrieved. |

**Returns**

A JSON array containing the active trace levels for the given `hgclass`.

**Notes**

The returned JSON array must be freed by the caller using `json_decref()`.

---

(gobj_get_gclass_trace_level_list)=
## `gobj_get_gclass_trace_level_list()`

Retrieves a list of trace levels set for a given `gclass`. If `gclass` is NULL, it returns the trace levels for all registered `gclass` instances.

```C
PUBLIC json_t *gobj_get_gclass_trace_level_list(
    hgclass gclass
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The `gclass` whose trace levels are to be retrieved. If NULL, retrieves trace levels for all `gclass` instances. |

**Returns**

A JSON array containing objects with `gclass` names and their respective trace levels.

**Notes**

The returned JSON array must be freed by the caller using `json_decref()`.

---

(gobj_get_gclass_trace_no_level)=
## `gobj_get_gclass_trace_no_level()`

Retrieves the trace levels that are explicitly disabled for a given gclass.

```C
PUBLIC json_t *gobj_get_gclass_trace_no_level(
    hgclass gclass
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The gclass whose disabled trace levels are to be retrieved. |

**Returns**

A JSON array containing the names of the disabled trace levels for the specified gclass.

**Notes**

This function returns a list of trace levels that have been explicitly disabled for the given gclass. The returned JSON array must be freed by the caller.

---

(gobj_get_gclass_trace_no_level_list)=
## `gobj_get_gclass_trace_no_level_list()`

Retrieves a list of gclasses with their respective trace levels that are explicitly disabled.

```C
PUBLIC json_t *gobj_get_gclass_trace_no_level_list(hgclass gclass_);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass_` | `hgclass` | The gclass to retrieve the disabled trace levels for. If NULL, retrieves the list for all gclasses. |

**Returns**

A JSON array containing objects with gclass names and their respective disabled trace levels.

**Notes**

If `gclass_` is NULL, the function iterates over all registered gclasses and returns their disabled trace levels.

---

(gobj_get_global_trace_level)=
## `gobj_get_global_trace_level()`

Retrieves the current global trace levels as a JSON array of strings.

```C
json_t *gobj_get_global_trace_level(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A JSON array containing the names of the currently enabled global trace levels.

**Notes**

The returned JSON object must be managed properly to avoid memory leaks. Use `json_decref()` when done.

---

(gobj_get_gobj_trace_level)=
## `gobj_get_gobj_trace_level()`

Retrieves the trace levels set for the specified `hgobj`. The function returns a JSON array containing the active trace levels.

```C
PUBLIC json_t *gobj_get_gobj_trace_level(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose trace levels are to be retrieved. |

**Returns**

A JSON array containing the active trace levels for the given `hgobj`.

**Notes**

If `gobj` is `NULL`, the function returns the global trace levels.

---

(gobj_get_gobj_trace_level_tree)=
## `gobj_get_gobj_trace_level_tree()`

Retrieves the trace levels set for a given gobj and its entire child tree.

```C
json_t *gobj_get_gobj_trace_level_tree(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The gobj whose trace levels and child tree trace levels are to be retrieved. |

**Returns**

A JSON array containing the trace levels of the specified gobj and all its child gobjs.

**Notes**

This function iterates over the entire child tree of the given gobj and collects their trace levels.

---

(gobj_get_gobj_trace_no_level)=
## `gobj_get_gobj_trace_no_level()`

Retrieves the trace levels that are explicitly disabled for the given `gobj`.

```C
json_t *gobj_get_gobj_trace_no_level(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance whose disabled trace levels are to be retrieved. |

**Returns**

A JSON array containing the names of the disabled trace levels for the given `gobj`. The caller does not own the returned JSON object.

**Notes**

This function returns a list of trace levels that have been explicitly disabled for the given `gobj`. The returned JSON object should not be modified or freed by the caller.

---

(gobj_get_gobj_trace_no_level_tree)=
## `gobj_get_gobj_trace_no_level_tree()`

Retrieves a hierarchical list of trace levels that are explicitly disabled for a given gobj and its child tree.

```C
PUBLIC json_t *gobj_get_gobj_trace_no_level_tree(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The gobj whose trace levels are to be retrieved, including its child tree. |

**Returns**

A JSON array containing objects with the gobj name and its disabled trace levels.

**Notes**

This function traverses the entire gobj tree and collects trace levels that have been explicitly disabled.

---

(gobj_get_trace_filter)=
## `gobj_get_trace_filter()`

Retrieves the trace filter configuration for a given `hgclass`.

```C
PUBLIC json_t *gobj_get_trace_filter(hgclass gclass);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The `hgclass` whose trace filter configuration is to be retrieved. |

**Returns**

A JSON object containing the trace filter configuration. The returned object is not owned by the caller.

**Notes**

The function returns the trace filter settings applied to the specified `hgclass`. The caller should not modify or free the returned JSON object.

---

(gobj_load_trace_filter)=
## `gobj_load_trace_filter()`

Loads a trace filter into the specified `hgclass`, replacing any existing filter.

```C
int gobj_load_trace_filter(
    hgclass gclass, 
    json_t *jn_trace_filter // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The `hgclass` instance where the trace filter will be applied. |
| `jn_trace_filter` | `json_t *` | A JSON object containing the trace filter rules. This parameter is owned and will be managed internally. |

**Returns**

Returns `0` on success.

**Notes**

This function replaces any existing trace filter in the specified `hgclass`.

---

(gobj_remove_trace_filter)=
## `gobj_remove_trace_filter()`

Removes a trace filter from the specified `hgclass`. If `attr` is empty, all filters are removed. If `value` is empty, all values for the given attribute are removed.

```C
int gobj_remove_trace_filter(
    hgclass gclass, 
    const char *attr, 
    const char *value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The `hgclass` from which the trace filter should be removed. |
| `attr` | `const char *` | The attribute name whose filter should be removed. If empty, all filters are removed. |
| `value` | `const char *` | The specific value to remove from the filter. If empty, all values for the given attribute are removed. |

**Returns**

Returns `0` on success, or `-1` if the attribute or value is not found.

**Notes**

If the last value of an attribute is removed, the attribute itself is also removed from the trace filter.

---

(gobj_repr_gclass_trace_levels)=
## `gobj_repr_gclass_trace_levels()`

Returns a JSON array containing trace level information for all registered GClasses or a specific GClass if a name is provided.

```C
json_t *gobj_repr_gclass_trace_levels(
    const char *gclass_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass_name` | `const char *` | The name of the GClass to retrieve trace levels for. If NULL, retrieves trace levels for all registered GClasses. |

**Returns**

A JSON array containing objects with GClass names and their respective trace levels.

**Notes**

If `gclass_name` is NULL, the function iterates over all registered GClasses and includes their trace levels in the output.

---

(gobj_repr_global_trace_levels)=
## `gobj_repr_global_trace_levels()`

Returns a JSON array containing the global trace levels and their descriptions.

```C
json_t *gobj_repr_global_trace_levels(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A JSON array where each element is an object containing the trace level name and its description.

**Notes**

This function provides a list of predefined global trace levels used for debugging and logging.

---

(gobj_set_deep_tracing)=
## `gobj_set_deep_tracing()`

Sets the deep tracing level for all gobjs, controlling the verbosity of trace logs.

```C
int gobj_set_deep_tracing(int level);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `level` | `int` | The tracing depth level. A value of 1 enables tracing while considering `__gobj_trace_no_level__`, and values greater than 1 enable full tracing. |

**Returns**

Returns 0 on success.

**Notes**

This function is useful for debugging and monitoring gobj behavior at different levels of detail.

---

(gobj_set_gclass_no_trace)=
## `gobj_set_gclass_no_trace()`

Sets or resets the no-trace level for a given `hgclass`.

```C
int gobj_set_gclass_no_trace(
    hgclass gclass, 
    const char *level, 
    BOOL set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The `hgclass` whose no-trace level is being modified. |
| `level` | `const char *` | The trace level to set or reset. If empty, all levels are affected. |
| `set` | `BOOL` | If `TRUE`, the level is set; if `FALSE`, the level is reset. |

**Returns**

Returns `0` on success, or `-1` if the specified trace level is not found.

**Notes**

If `level` is empty, all trace levels are affected. If `gclass` is `NULL`, an error is logged.

---

(gobj_set_gclass_trace)=
## `gobj_set_gclass_trace()`

Sets or resets the trace level for a given `hgclass`. If `level` is `NULL`, all trace levels are set or reset. If `level` is an empty string, only user-defined trace levels are affected.

```C
int gobj_set_gclass_trace(
    hgclass gclass,
    const char *level,
    BOOL set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The `hgclass` whose trace level is being modified. |
| `level` | `const char *` | The trace level to set or reset. If `NULL`, all levels are affected. If an empty string, only user-defined levels are affected. |
| `set` | `BOOL` | If `TRUE`, the trace level is set; if `FALSE`, it is reset. |

**Returns**

Returns `0` on success, or `-1` if the specified trace level is not found.

**Notes**

If `gclass` is `NULL`, the function modifies the global trace level instead.

---

(gobj_set_global_no_trace)=
## `gobj_set_global_no_trace()`

Sets or resets the global no-trace level for debugging and logging control.

```C
int gobj_set_global_no_trace(
    const char *level,
    BOOL set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `level` | `const char *` | The trace level to be set or reset. If empty, all global trace levels are affected. |
| `set` | `BOOL` | If TRUE, the specified trace level is set; if FALSE, it is reset. |

**Returns**

Returns 0 on success, or -1 if the specified trace level is not found.

**Notes**

This function modifies the global trace level settings, affecting all objects in the system.

---

(gobj_set_global_trace)=
## `gobj_set_global_trace()`

Sets or resets the global trace level for debugging and logging purposes.

```C
int gobj_set_global_trace(
    const char *level,
    BOOL        set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `level` | `const char *` | The trace level to set or reset. If empty, all global trace levels are affected. |
| `set` | `BOOL` | If `TRUE`, the trace level is set; if `FALSE`, the trace level is reset. |

**Returns**

Returns 0 on success, or -1 if the specified trace level is not found.

**Notes**

If `level` is empty, all global trace levels are affected. The function ensures that the trace level exists before modifying it.

---

(gobj_set_gobj_no_trace)=
## `gobj_set_gobj_no_trace()`

Sets or resets the no-trace level for a given `gobj`. This function modifies the trace level settings to exclude specific trace levels from being logged.

```C
int gobj_set_gobj_no_trace(
    hgobj gobj,
    const char *level,
    BOOL set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `gobj` whose no-trace level is being modified. |
| `level` | `const char *` | The trace level to be set or reset. If empty, all trace levels are affected. |
| `set` | `BOOL` | If `TRUE`, the specified trace level is added to the no-trace list; if `FALSE`, it is removed. |

**Returns**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

This function prevents specific trace levels from being logged for the given `gobj`. If `level` is empty, all trace levels are affected.

---

(gobj_set_gobj_trace)=
## `gobj_set_gobj_trace()`

Sets or resets the trace level for a given `hgobj`. If `gobj` is `NULL`, it modifies the global trace level instead. Calls [`mt_trace_on()`](#mt_trace_on) or [`mt_trace_off()`](#mt_trace_off) if applicable.

```C
int gobj_set_gobj_trace(
    hgobj gobj,
    const char *level,
    BOOL set,
    json_t *kw
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` whose trace level is being modified. If `NULL`, modifies the global trace level. |
| `level` | `const char *` | The trace level to set or reset. If `NULL`, all levels are affected. If an empty string, only user-defined levels are affected. |
| `set` | `BOOL` | If `TRUE`, enables the specified trace level; if `FALSE`, disables it. |
| `kw` | `json_t *` | Additional parameters (owned). |

**Returns**

Returns `0` on success, or `-1` if the specified trace level is invalid.

**Notes**

If `gobj` is not `NULL`, this function invokes [`mt_trace_on()`](#mt_trace_on) or [`mt_trace_off()`](#mt_trace_off) if they are defined in the object's gclass.

---

(gobj_trace_level)=
## `gobj_trace_level()`

Returns the trace level bitmask for the given `hgobj`.

```C
PUBLIC uint32_t gobj_trace_level(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose trace level is to be retrieved. |

**Returns**

A 32-bit bitmask representing the trace levels enabled for the given `hgobj`.

**Notes**

If `gobj` is `NULL`, the function returns the global trace level bitmask.

---

(gobj_trace_level_list)=
## `gobj_trace_level_list()`

Returns a JSON object containing the trace levels available for a given `hgclass`.

```C
PUBLIC json_t *gobj_trace_level_list(
    hgclass gclass
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass` | `hgclass` | The `hgclass` whose trace levels are to be retrieved. |

**Returns**

A JSON object containing the trace levels available for the specified `hgclass`.

**Notes**

The returned JSON object must be decremented when no longer needed.

---

(gobj_trace_no_level)=
## `gobj_trace_no_level()`

Retrieves the trace level mask that is explicitly disabled for the given `hgobj`.

```C
PUBLIC uint32_t gobj_trace_no_level(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose disabled trace levels are to be retrieved. |

**Returns**

Returns a 32-bit mask representing the disabled trace levels for the given `hgobj`.

**Notes**

The function combines the global disabled trace levels with those specific to the `hgobj` and its [`gclass`](#gclass).

---

(is_level_not_tracing)=
## `is_level_not_tracing()`

Determines whether a given trace level is explicitly disabled for a specified `hgobj` instance or globally.

```C
PUBLIC BOOL is_level_not_tracing(
    hgobj gobj_,
    uint32_t level
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_` | `hgobj` | The object whose trace level is being checked. If NULL, the global trace level is considered. |
| `level` | `uint32_t` | The trace level to check. |

**Returns**

Returns `TRUE` if the specified trace level is disabled, otherwise returns `FALSE`.

**Notes**

If `__deep_trace__` is greater than 1, tracing is always enabled, overriding any disabled levels.

---

(is_level_tracing)=
## `is_level_tracing()`

Checks if the given `gobj` has the specified trace `level` enabled, considering global and class-level trace settings.

```C
PUBLIC BOOL is_level_tracing(
    hgobj gobj, 
    uint32_t level
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance to check for the trace level. |
| `level` | `uint32_t` | The trace level bitmask to check. |

**Returns**

Returns `TRUE` if the trace level is enabled for the given `gobj`, otherwise returns `FALSE`.

**Notes**

If `gobj` is `NULL`, the function checks the global trace level. The function considers deep tracing settings and class-level trace configurations.

---

(tab)=
## `tab()`

Generates an indentation string based on the current depth level of nested function calls.

```C
char *tab(
    char *bf,
    int   bflen
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer to store the generated indentation string. |
| `bflen` | `int` | Maximum length of the buffer. |

**Returns**

A pointer to the buffer containing the generated indentation string.

**Notes**

The indentation is determined by the global variable `__inside__`, which tracks the depth level of nested function calls.

---

(trace_machine)=
## `trace_machine()`

The `trace_machine()` function logs formatted trace messages for debugging purposes.

```C
void trace_machine(const char *fmt, ...);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fmt` | `const char *` | The format string, similar to `printf()`, specifying the message format. |
| `...` | `variadic arguments` | Additional arguments corresponding to the format specifiers in `fmt`. |

**Returns**

This function does not return a value.

**Notes**

This function is used for debugging and tracing execution flow. It formats and logs messages based on the provided format string and arguments.

---
