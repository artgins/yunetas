# Command Parser Functions

Source code in:

- [command_parser.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/command_parser.c)
- [command_parser.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/command_parser.h)

(authz_get_level_desc)=
## `authz_get_level_desc()`

The `authz_get_level_desc()` function searches for an authorization level descriptor in the given `authz_table` based on the provided `auth` name.

```C
const sdata_desc_t *authz_get_level_desc(
    const sdata_desc_t *authz_table,
    const char *auth
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `authz_table` | `const sdata_desc_t *` | Pointer to the authorization table containing authorization descriptors. |
| `auth` | `const char *` | The name of the authorization level to search for. |

**Returns**

Returns a pointer to the matching `sdata_desc_t` descriptor if found, otherwise returns `NULL`.

**Notes**

The function first checks for an alias match if no direct match is found. If an alias exists and matches `auth`, the corresponding descriptor is returned.

---

(authzs_list)=
## `authzs_list()`

`authzs_list()` retrieves a list of authorization descriptors for a given `hgobj`. If an `authz` name is provided, it returns the specific authorization descriptor matching that name.

```C
json_t *authzs_list(
    hgobj       gobj,
    const char *authz
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The object whose authorization descriptors are to be retrieved. Can be `NULL` to retrieve global authorizations. |
| `authz` | `const char *` | The name of a specific authorization descriptor to retrieve. If empty, all available authorizations are returned. |

**Returns**

A `json_t *` object containing the list of authorization descriptors. If `authz` is provided, returns the specific descriptor or `NULL` if not found.

**Notes**

If `gobj` is `NULL`, the function retrieves global authorization descriptors. If `authz` is not found, an error is logged.

---

(command_parser)=
## `command_parser()`

`command_parser()` processes a command string, expands its parameters, checks authorization, and executes the corresponding function or event.

```C
json_t *command_parser(
    hgobj       gobj,
    const char  *command,
    json_t      *kw,
    hgobj       src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj handling the command execution. |
| `command` | `const char *` | The command string to be parsed and executed. |
| `kw` | `json_t *` | A JSON object containing additional parameters for the command. |
| `src` | `hgobj` | The source GObj that issued the command. |

**Returns**

A JSON object containing the command execution result, or `NULL` if the response is asynchronous.

**Notes**

If the command is not found, an error response is returned.
If the command requires authorization, it is checked before execution.
If the command has a function handler, it is executed directly.
If the command does not have a function handler, it is redirected as an event.

---

(gobj_build_authzs_doc)=
## `gobj_build_authzs_doc()`

`gobj_build_authzs_doc()` generates a JSON object describing the authorization levels available for a given service or globally.

```C
json_t *gobj_build_authzs_doc(
    hgobj      gobj,
    const char *cmd,
    json_t     *kw
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance from which to retrieve authorization levels. |
| `cmd` | `const char *` | The command name, currently unused in the function. |
| `kw` | `json_t *` | A JSON object containing optional parameters: 'authz' to filter by authorization level and 'service' to specify a particular service. |

**Returns**

A JSON object containing the authorization levels for the specified service or globally. If a specific authorization level is requested and not found, an error message is returned as a JSON string.

**Notes**

If 'service' is provided in `kw`, the function retrieves the authorization levels for that service. If 'authz' is specified, it filters the results to include only the requested authorization level.

---

(gobj_build_cmds_doc)=
## `gobj_build_cmds_doc()`

`gobj_build_cmds_doc()` generates a JSON-formatted documentation of available commands for a given `hgobj`.

```C
json_t *gobj_build_cmds_doc(
    hgobj   gobj,
    json_t *kw
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose commands are to be documented. |
| `kw` | `json_t *` | A JSON object containing optional parameters such as `level` (integer) to control the depth of command retrieval and `cmd` (string) to filter a specific command. |

**Returns**

A JSON string containing the formatted documentation of available commands. If a specific command is requested and found, its detailed documentation is returned. If the command is not found, an error message is returned.

**Notes**

If `level` is set, [`gobj_build_cmds_doc()`](#gobj_build_cmds_doc) will also include commands from child objects of the given `hgobj`.

---
