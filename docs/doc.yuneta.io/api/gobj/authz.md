# Authorization

Authorization checks and helpers used by the control plane. A gobj declares an `authorization_checker` callback at startup and the framework consults it for every command or stats call.

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)
- [`c_authz.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/root-linux/src/c_authz.h)

(gobj_authenticate)=
## `gobj_authenticate()`

The `gobj_authenticate()` function authenticates a user based on the provided JSON parameters. If no authentication parser is set, it defaults to the system user.

```C
json_t *gobj_authenticate(
    hgobj gobj,
    json_t *kw,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance performing the authentication. |
| `kw` | `json_t *` | A JSON object containing authentication parameters. Ownership is transferred. |
| `src` | `hgobj` | The source GObj requesting authentication. |

**Returns**

Returns a JSON object containing the authentication result. The object includes `result` (0 for success, -1 for failure), `comment` (a message), and `username` (the authenticated username).

**Notes**

If a local authentication method (`mt_authenticate`) is defined, it takes precedence. Otherwise, a global authentication parser is used. If neither is available, the function defaults to the system user.

---

(gobj_authz)=
## `gobj_authz()`

Retrieves the authorization details for a given `hgobj`. If `authz` is specified, it returns the details of that specific authorization; otherwise, it returns all authorizations available for the object.

```C
json_t *gobj_authz(
    hgobj gobj,
    const char *authz
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The object whose authorizations are being queried. |
| `authz` | `const char *` | The specific authorization to retrieve. If empty, all authorizations are returned. |

**Returns**

A JSON object containing the authorization details. Returns `NULL` if the object is `NULL` or destroyed.

**Notes**

This function internally calls `authzs_list()` to fetch the authorization details.

---

(gobj_authzs)=
## `gobj_authzs()`

Returns a list of authorization levels available for the given `hgobj`. If `gobj` is `NULL`, it returns the global authorization levels.

```C
json_t *gobj_authzs(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose authorization levels are to be retrieved. If `NULL`, global authorization levels are returned. |

**Returns**

A JSON object containing the list of authorization levels available for the given `hgobj` or globally if `gobj` is `NULL`.

**Notes**

This function is useful for inspecting the authorization levels assigned to a specific `hgobj` or retrieving the global authorization levels.

---

(gobj_get_global_authz_table)=
## `gobj_get_global_authz_table()`

Retrieves the global authorization table, which defines the available authorization levels and their corresponding permissions.

```C
const sdata_desc_t *gobj_get_global_authz_table(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A pointer to a `sdata_desc_t` structure representing the global authorization table.

**Notes**

The returned table contains predefined authorization levels such as `__read_attribute__`, `__write_attribute__`, and `__execute_command__`, among others.

---

(gobj_user_has_authz)=
## `gobj_user_has_authz()`

Checks if a user has the specified authorization level in the context of the given `hgobj`. If no authorization checker is defined, the function defaults to granting authorization.

```C
BOOL gobj_user_has_authz(
    hgobj gobj,
    const char *authz,
    json_t *kw,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance in which the authorization check is performed. |
| `authz` | `const char *` | The name of the authorization level to check. |
| `kw` | `json_t *` | A JSON object containing additional parameters for the authorization check. |
| `src` | `hgobj` | The source `hgobj` requesting the authorization check. |

**Returns**

Returns `TRUE` if the user has the required authorization, otherwise returns `FALSE`.

**Notes**

If the `hgobj` has a local authorization checker (`mt_authz_checker`), it is used first. If not, the global authorization checker is used. If neither is defined, the function defaults to returning `TRUE`.

---

(authz_get_level_desc)=
## `authz_get_level_desc()`

Looks up an authorization level descriptor by name within an authorization table. The search matches against both the `name` field and any `alias` entries in each descriptor. When a descriptor has no `json_fn` callback, aliases take precedence over the name match.

```C
const sdata_desc_t *authz_get_level_desc(
    const sdata_desc_t *authz_table,
    const char *authz
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `authz_table` | `const sdata_desc_t *` | A null-terminated array of `sdata_desc_t` descriptors representing the authorization table to search. |
| `authz` | `const char *` | The authorization name (or alias) to look up. |

**Returns**

A pointer to the matching `sdata_desc_t` entry if found, or `NULL` if no entry matches.

**Notes**

Aliases allow a single authorization level to be referenced by multiple names. When a descriptor has no `json_fn`, alias matching is checked first, allowing aliases to redirect authorization checks to a named event.

---

(gobj_build_authzs_doc)=
## `gobj_build_authzs_doc()`

Builds a JSON document describing available authorizations. If a `service` is specified in `kw`, returns the authorization list for that service only. Otherwise, returns a comprehensive document containing the global authorization table plus the authorization tables of all registered services that define one.

```C
json_t *gobj_build_authzs_doc(
    hgobj gobj,
    const char *cmd,
    json_t *kw
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj context (used for logging). |
| `cmd` | `const char *` | The command name that triggered this call (informational). |
| `kw` | `json_t *` | A JSON object with optional keys: `"service"` (string -- restrict to a specific service) and `"authz"` (string -- filter by a specific authorization name). |

**Returns**

A new JSON value (owned by the caller). If a specific service was requested, returns its authz list (array or object). If no service was specified, returns a JSON object keyed by `"global authzs"` and each service name, with their respective authorization lists as values. Returns a JSON string with an error message if the service is not found or has no authorization table.

**Notes**

This function is typically invoked as part of a command handler to provide introspection into the authorization system.

---

(authzs_list)=
## `authzs_list()`

Returns the authorization descriptors for a gobj's GClass. If `authz` is empty, returns the full list of all authorization entries as a JSON array. If `authz` is a specific name, returns only the matching authorization entry as a JSON object. When `gobj` is `NULL`, the global authorization table is used instead.

```C
json_t *authzs_list(
    hgobj gobj,
    const char *authz
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance whose GClass authorization table will be queried. Pass `NULL` to query the global authorization table. |
| `authz` | `const char *` | The specific authorization name to look up. Pass an empty string to return all authorizations. |

**Returns**

A new JSON value (owned by the caller): a JSON array of all authorization entries when `authz` is empty, or a single JSON object for the matching entry. Returns `NULL` if the gobj's GClass has no authorization table or if the specified `authz` is not found.

**Notes**

The authorization descriptors are converted from the `sdata_desc_t` format to JSON via `sdataauth2json()`. An error is logged if a specific `authz` name is requested but not found.

---


(authz_checker)=
## `authz_checker()`

Default authorization checker. Used when no custom `authz_checker` is
provided to [`yuneta_setup()`](../runtime/entry_point.md#yuneta_setup).

```C
BOOL authz_checker(
    hgobj gobj_to_check,
    const char *authz,
    json_t *kw,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_to_check` | `hgobj` | GObject on which authorization is checked. |
| `authz` | `const char *` | Authorization rule / permission name. |
| `kw` | `json_t *` | Context keyword arguments. |
| `src` | `hgobj` | Source gobj requesting authorization. |

**Returns**

`TRUE` if authorized, `FALSE` otherwise.

---

(api-authentication_parser)=
## `authentication_parser()`

Default authentication parser. Used when no custom `authentication_parser`
is provided to [`yuneta_setup()`](../runtime/entry_point.md#yuneta_setup).

```C
json_t *authentication_parser(
    hgobj gobj_service,
    json_t *kw,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_service` | `hgobj` | Service gobj. |
| `kw` | `json_t *` | Keyword arguments with authentication data. |
| `src` | `hgobj` | Source gobj. |

**Returns**

JSON object with parsed authentication info, or `NULL`.

---
