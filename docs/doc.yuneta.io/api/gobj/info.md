# Info

Introspection helpers: retrieve a gobj's name, short name, full name, parent, yuno, and other metadata used by tooling and logs.

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)

(get_attrs_schema)=
## `get_attrs_schema()`

Returns a JSON array describing the attributes of a given `hgobj`. The attributes included are those marked with `SDF_RD`, `SDF_WR`, `SDF_STATS`, `SDF_PERSIST`, `SDF_VOLATIL`, `SDF_RSTATS`, or `SDF_PSTATS`.

```C
json_t *get_attrs_schema(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` whose attributes are to be described. |

**Returns**

A JSON array where each element is an object containing details about an attribute, including its name, type, flags, and description.

**Notes**

The returned JSON array must be freed by the caller using `json_decref()`.

---

(get_sdata_flag_table)=
## `get_sdata_flag_table()`

Returns a table of sdata (structured data) flag names as a null-terminated array of strings.

```C
const char **get_sdata_flag_table(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A pointer to a null-terminated array of strings representing sdata flag names.

**Notes**

The returned array contains predefined flag names used in structured data attributes. The caller should not modify or free the returned pointer.

---

(gobj2json)=
## `gobj2json()`

Returns a JSON object containing a structured description of the given `gobj`, including attributes, state, and metadata.

```C
json_t *gobj2json(
    hgobj gobj,
    json_t *jn_filter // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance to be described. |
| `jn_filter` | `json_t *` | A JSON list specifying which attributes to include in the output. |

**Returns**

A JSON object containing the requested details of `gobj`. The caller owns the returned JSON object and must free it when done.

**Notes**

The `jn_filter` parameter allows selective inclusion of attributes such as `fullname`, `state`, `attrs`, and `gobj_flags`. If `jn_filter` is empty, all available attributes are included.

---

(gobj_command_desc)=
## `gobj_command_desc()`

Retrieves the command description for a given gobj. If the command name is NULL, it returns the full command table.

```C
const sdata_desc_t *gobj_command_desc(
    hgobj gobj,
    const char *name,
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The gobj instance whose command description is to be retrieved. |
| `name` | `const char *` | The name of the command. If NULL, the full command table is returned. |
| `verbose` | `BOOL` | If TRUE, logs an error message when the command is not found. |

**Returns**

A pointer to the `sdata_desc_t` structure describing the command, or NULL if the command is not found.

**Notes**

This function first checks if the `gobj` is valid. If the command name is NULL, it returns the full command table. Otherwise, it searches for the specific command in the gobj's gclass.

---

(gobj_full_name)=
## `gobj_full_name()`

Returns the full hierarchical name of the given `hgobj`, constructed from its ancestors using the '`' separator.

```C
const char *gobj_full_name(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` whose full name is to be retrieved. |

**Returns**

A pointer to a statically allocated string containing the full hierarchical name of the `hgobj`. Returns '???' if the object is NULL or destroyed.

**Notes**

The returned string is managed internally and should not be modified or freed by the caller.

---

(gobj_gclass)=
## `gobj_gclass()`

Returns the gclass associated with the given `hgobj`.

```C
hgclass gobj_gclass(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` whose gclass is to be retrieved. |

**Returns**

Returns the `hgclass` associated with the given `hgobj`, or NULL if the input is invalid.

**Notes**

This function does not perform extensive validation on the input `hgobj`. Ensure that the object is properly initialized before calling this function.

---

(gobj_gclass_name)=
## `gobj_gclass_name()`

Retrieves the gclass name associated with the given `hgobj`.

```C
gclass_name_t gobj_gclass_name(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose gclass name is to be retrieved. |

**Returns**

Returns the gclass name of the given `hgobj`. If `gobj` is `NULL`, returns `???`.

**Notes**

This function does not perform any validation beyond checking for `NULL`.

---

(gobj_global_variables)=
## `gobj_global_variables()`

Returns a JSON object containing global variables related to the Yuno environment, including system, realm, and node attributes.

```C
json_t *gobj_global_variables(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A JSON object containing the following global variables:

| **Variable**              | **Description**                          |
|---------------------------|------------------------------------------|
| `__node_owner__`          | Node owner of the Yuno.                  |
| `__realm_id__`            | Realm ID of the Yuno.                    |
| `__realm_owner__`         | Realm owner of the Yuno.                 |
| `__realm_role__`          | Realm role of the Yuno.                  |
| `__realm_name__`          | Name of the realm.                       |
| `__realm_env__`           | Environment of the realm.                |
| `__yuno_id__`             | Unique ID of the Yuno.                   |
| `__yuno_role__`           | Role of the Yuno.                        |
| `__yuno_name__`           | Name of the Yuno.                        |
| `__yuno_tag__`            | Tag of the Yuno.                         |
| `__yuno_role_plus_name__` | Role and name of the Yuno.               |
| `__hostname__`            | Hostname of the system.                  |
| `__sys_system_name__`     | System name (Linux only).                |
| `__sys_node_name__`       | Node name (Linux only).                  |
| `__sys_version__`         | System version (Linux only).             |
| `__sys_release__`         | System release (Linux only).             |
| `__sys_machine__`         | Machine type (Linux only).               |
| `__tls_library__`         | Active TLS backend: `"openssl"` or `"mbedtls"` (compile-time). |
| `__bind_ip__`             | Bind IP address of the Yuno (set when yuno is running). |
| `__multiple__`            | Whether the Yuno allows multiple instances (boolean, set when yuno is running). |

**Notes**

The returned JSON object must be decremented with `json_decref()` to avoid memory leaks. These variables are also available for substitution in configuration strings via the `(^^ ^^)` syntax — see [Settings](../../guide/settings.md#1-global-variable-substitution).

---

(gobj_is_destroying)=
## `gobj_is_destroying()`

Checks if the given `hgobj` is in the process of being destroyed or has already been destroyed.

```C
BOOL gobj_is_destroying(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance to check. |

**Returns**

Returns `TRUE` if the `hgobj` is being destroyed or has already been destroyed, otherwise returns `FALSE`.

**Notes**

This function is useful for preventing operations on objects that are no longer valid.

---

(gobj_is_disabled)=
## `gobj_is_disabled()`

Checks if the given `hgobj` is disabled. A disabled `hgobj` is prevented from starting or playing.

```C
BOOL gobj_is_disabled(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance to check. |

**Returns**

Returns `TRUE` if the `hgobj` is disabled, otherwise returns `FALSE`.

**Notes**

A disabled `hgobj` cannot be started or played until it is explicitly enabled using [`gobj_enable()`](<#gobj_enable>).

---

(gobj_is_playing)=
## `gobj_is_playing()`

Checks if the given `hgobj` is currently in a playing state.

```C
BOOL gobj_is_playing(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance to check. |

**Returns**

Returns `TRUE` if the `hgobj` is playing, otherwise returns `FALSE`.

**Notes**

If the `hgobj` is `NULL` or has been destroyed, an error is logged and `FALSE` is returned.

---

(gobj_is_pure_child)=
## `gobj_is_pure_child()`

Checks if the given `hgobj` is marked as a pure child, meaning it sends events directly to its parent instead of publishing them.

```C
BOOL gobj_is_pure_child(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance to check. |

**Returns**

Returns `TRUE` if the `hgobj` is a pure child, otherwise returns `FALSE`.

**Notes**

A pure child is a `hgobj` that bypasses event publication and directly sends events to its parent.

---

(gobj_is_running)=
## `gobj_is_running()`

Checks if the given `hgobj` instance is currently running.

```C
BOOL gobj_is_running(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance to check. |

**Returns**

Returns `TRUE` if the `hgobj` is running, otherwise returns `FALSE`.

**Notes**

If the provided `hgobj` is `NULL` or has been destroyed, an error is logged and `FALSE` is returned.

---

(gobj_is_service)=
## `gobj_is_service()`

Checks if the given `hgobj` is marked as a service. A service object is an interface available for external access.

```C
BOOL gobj_is_service(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The object to check. |

**Returns**

Returns `TRUE` if the object is a service, otherwise returns `FALSE`.

**Notes**

A service object is identified by the `gobj_flag_service` flag.

---

(gobj_is_volatil)=
## `gobj_is_volatil()`

Checks if the given `hgobj` is marked as volatile, meaning it is temporary and can be automatically destroyed.

```C
BOOL gobj_is_volatil(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance to check. |

**Returns**

Returns `TRUE` if the `hgobj` is volatile, otherwise returns `FALSE`.

**Notes**

A volatile `hgobj` is typically used for temporary objects that do not persist beyond their immediate use.

---

(gobj_name)=
## `gobj_name()`

Returns the name of the given `hgobj` instance as a string.

```C
const char *gobj_name(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose name is to be retrieved. |

**Returns**

A pointer to a string containing the name of the `hgobj` instance. If `gobj` is `NULL`, returns "???".

**Notes**

The returned string is managed internally and should not be modified or freed by the caller.

---

(gobj_parent)=
## `gobj_parent()`

Returns the parent object of the given `hgobj`.

```C
hgobj gobj_parent(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The object whose parent is to be retrieved. |

**Returns**

Returns the parent `hgobj` of the given object, or NULL if the object has no parent.

**Notes**

If the provided `hgobj` is NULL, an error is logged.

---

(gobj_priv_data)=
## `gobj_priv_data()`

Returns a pointer to the private data of the given `hgobj`.

```C
void *gobj_priv_data(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose private data is to be retrieved. |

**Returns**

A pointer to the private data associated with the given `hgobj`.

**Notes**

The returned pointer provides direct access to the private data structure of the `hgobj`. Ensure that the `hgobj` is valid before accessing its private data.

---

(gobj_set_volatil)=
## `gobj_set_volatil()`

Sets or clears the `gobj_flag_volatil` flag for the given `hgobj` instance.

```C
int gobj_set_volatil(
    hgobj gobj, 
    BOOL set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose volatility flag is to be modified. |
| `set` | `BOOL` | If `TRUE`, the `gobj_flag_volatil` flag is set; if `FALSE`, the flag is cleared. |

**Returns**

Returns `0` on success.

**Notes**

A volatile `hgobj` is typically used for temporary objects that should not persist beyond a certain scope.

---

(gobj_short_name)=
## `gobj_short_name()`

Returns the short name of the given `hgobj`, formatted as `gclass^name`.

```C
const char * gobj_short_name(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_` | `hgobj` | The object whose short name is to be retrieved. |

**Returns**

A pointer to a string containing the short name of the object. If the object is `NULL` or destroyed, returns `???`.

**Notes**

The short name is generated dynamically and cached for future calls.

---

(gobj_typeof_gclass)=
## `gobj_typeof_gclass()`

Checks if the given `hgobj` belongs to the specified gclass.

```C
BOOL gobj_typeof_gclass(
    hgobj gobj,
    const char *gclass_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The object whose gclass is to be checked. |
| `gclass_name` | `const char *` | The name of the gclass to compare against. |

**Returns**

Returns `TRUE` if the `hgobj` belongs to the specified gclass, otherwise returns `FALSE`.

**Notes**

This function performs a strict comparison against the object's gclass and does not check for inherited gclasses.

---

(gobj_typeof_inherited_gclass)=
## `gobj_typeof_inherited_gclass()`

Checks if the given `hgobj` belongs to a specific inherited gclass.

```C
BOOL gobj_typeof_inherited_gclass(
    hgobj gobj,
    const char *gclass_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance to check. |
| `gclass_name` | `const char *` | The name of the gclass to compare against. |

**Returns**

Returns `TRUE` if the `gobj` or any of its inherited (bottom) gobjs belong to the specified gclass, otherwise returns `FALSE`.

**Notes**

This function traverses the bottom gobj hierarchy to determine if any of them match the given `gclass_name`.

---

(gobj_view_tree)=
## `gobj_view_tree()`

Generates a hierarchical JSON representation of the given `hgobj` and its child objects, including selected attributes.

```C
json_t *gobj_view_tree(
    hgobj gobj,
    json_t *jn_filter // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The root `hgobj` whose tree structure will be represented. |
| `jn_filter` | `json_t *` | A JSON object specifying which attributes to include in the output. Owned by the function. |

**Returns**

A JSON object representing the hierarchical structure of `gobj` and its children, including the requested attributes.

**Notes**

The returned JSON object must be freed by the caller. The `jn_filter` parameter allows selective inclusion of attributes such as `fullname`, `state`, `attrs`, etc.

---

(gobj_yuno)=
## `gobj_yuno()`

Returns the root `gobj` instance representing the Yuno, which is the top-level object in the hierarchy.

```C
hgobj gobj_yuno(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A handle to the Yuno `gobj` instance, or `NULL` if the Yuno has not been initialized or has been destroyed.

**Notes**

The Yuno is the top-most `gobj` in the hierarchy and serves as the root of the object tree.

---

(gobj_yuno_id)=
## `gobj_yuno_id()`

Returns the unique identifier of the Yuno instance.

```C
const char *gobj_yuno_id(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A string representing the unique identifier of the Yuno instance.

**Notes**

If the Yuno instance is not initialized, an empty string is returned.

---

(gobj_yuno_name)=
## `gobj_yuno_name()`

Returns the name of the Yuno instance by retrieving the `name` attribute from [`gobj_yuno()`](#gobj_yuno).

```C
const char *gobj_yuno_name(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A string representing the name of the Yuno instance. Returns an empty string if the Yuno instance is not available.

**Notes**

['This function retrieves the `name` attribute from the Yuno instance returned by [`gobj_yuno()`](#gobj_yuno).', 'If [`gobj_yuno()`](#gobj_yuno) returns `NULL`, an empty string is returned.']

---

(gobj_yuno_node_owner)=
## `gobj_yuno_node_owner()`

Returns the node owner of the Yuno instance as a string.

```C
const char *gobj_yuno_node_owner(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A pointer to a string containing the node owner of the Yuno instance. If the Yuno instance is not available, an empty string is returned.

**Notes**

This function retrieves the value of the `node_owner` attribute from the Yuno instance.

---

(gobj_yuno_realm_env)=
## `gobj_yuno_realm_env()`

Returns the realm environment of the Yuno instance.

```C
const char *gobj_yuno_realm_env(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A string representing the realm environment of the Yuno instance. Returns an empty string if the Yuno instance is not available.

**Notes**

This function retrieves the value of the `realm_env` attribute from the Yuno instance.

---

(gobj_yuno_realm_id)=
## `gobj_yuno_realm_id()`

Returns the realm ID of the Yuno instance.

```C
const char *gobj_yuno_realm_id(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A string representing the realm ID of the Yuno instance. Returns an empty string if the Yuno instance is not available.

**Notes**

The function retrieves the value from the `realm_id` attribute of the Yuno object, which is the root object in the GObj hierarchy.

---

(gobj_yuno_realm_name)=
## `gobj_yuno_realm_name()`

Returns the realm name of the Yuno instance.

```C
const char *gobj_yuno_realm_name(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A string representing the realm name of the Yuno instance. Returns an empty string if the Yuno instance is not available.

**Notes**

The function retrieves the value of the `realm_name` attribute from the Yuno instance.

---

(gobj_yuno_realm_owner)=
## `gobj_yuno_realm_owner()`

Returns the realm owner of the current Yuno instance.

```C
const char *gobj_yuno_realm_owner(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A string representing the realm owner of the Yuno instance. If the Yuno instance is not available, an empty string is returned.

**Notes**

The function retrieves the value from the `realm_owner` attribute of the Yuno instance.

---

(gobj_yuno_realm_role)=
## `gobj_yuno_realm_role()`

Returns the realm role of the current `yuno` instance.

```C
const char *gobj_yuno_realm_role(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A string representing the realm role of the `yuno` instance. If the `yuno` instance is not available, an empty string is returned.

**Notes**

['The function retrieves the value of the `realm_role` attribute from the `yuno` instance.', 'If the `yuno` instance is not initialized, it returns an empty string.']

---

(gobj_yuno_role)=
## `gobj_yuno_role()`

Returns the role of the current Yuno instance as a string.

```C
const char *gobj_yuno_role(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A string representing the role of the Yuno instance. If the Yuno instance is not available, an empty string is returned.

**Notes**

The function retrieves the value of the `yuno_role` attribute from the Yuno instance using [`gobj_read_str_attr()`](<#gobj_read_str_attr>).

---

(gobj_yuno_role_plus_name)=
## `gobj_yuno_role_plus_name()`

Returns the concatenated role and name of the Yuno instance.

```C
const char *gobj_yuno_role_plus_name(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A string containing the Yuno's role and name concatenated.

**Notes**

If the Yuno instance is not initialized, an empty string is returned.

---

(gobj_yuno_tag)=
## `gobj_yuno_tag()`

Returns the tag of the current `yuno` instance as a string.

```C
const char *gobj_yuno_tag(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A pointer to a string containing the `yuno` tag. If the `yuno` instance is not available, an empty string is returned.

**Notes**

The returned string is managed internally and should not be modified or freed by the caller.

---

(gobj_audit_commands)=
## `gobj_audit_commands()`

*Description pending — signature extracted from header.*

```C
int gobj_audit_commands(
    int (*audit_command_cb)( const char *command, json_t *kw, void *user_data ),
    void *user_data
);
```

---

(gobj_is_bottom_gobj)=
## `gobj_is_bottom_gobj()`

*Description pending — signature extracted from header.*

```C
BOOL gobj_is_bottom_gobj(
    hgobj gobj
);
```

---

(gobj_is_top_service)=
## `gobj_is_top_service()`

*Description pending — signature extracted from header.*

```C
BOOL gobj_is_top_service(
    hgobj gobj
);
```

---

(gobj_kw_get_user_data)=
## `gobj_kw_get_user_data()`

*Description pending — signature extracted from header.*

```C
json_t *gobj_kw_get_user_data(
    hgobj gobj,
    const char *path,
    json_t *default_value,
    kw_flag_t flag
);
```

---

(gobj_nearest_top_service)=
## `gobj_nearest_top_service()`

*Description pending — signature extracted from header.*

```C
hgobj gobj_nearest_top_service(
    hgobj gobj
);
```

---

(gobj_top_services)=
## `gobj_top_services()`

*Description pending — signature extracted from header.*

```C
json_t *gobj_top_services(void);
```

---

