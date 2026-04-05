# Authz Functions

Source code in:
- [gobj.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)
- [gobj.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)

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
