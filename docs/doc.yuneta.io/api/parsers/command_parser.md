# Command Parser

Default parser for the control-plane `command` verb. Dispatches a textual command (`help`, `stats`, custom commands…) to the gobj that declared it, handling parameter parsing and authorization.

Source code:

- [`command_parser.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/command_parser.h)
- [`command_parser.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/command_parser.c)

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

(build_command_response)=
## `build_command_response()`

`build_command_response()` builds a standardized JSON response object for command and stats operations. The response contains four fields: `result`, `comment`, `schema`, and `data`.

```C
json_t *build_command_response(
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment,
    json_t *jn_schema,
    json_t *jn_data
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance associated with the response (currently unused in the response body). |
| `result` | `json_int_t` | Numeric result code. Use `0` for success and `-1` (or other negative values) for errors. |
| `jn_comment` | `json_t *` | **Owned.** A JSON string with a human-readable message. If `NULL`, defaults to an empty string `""`. |
| `jn_schema` | `json_t *` | **Owned.** A JSON value describing the schema of the returned data. If `NULL`, defaults to `json_null()`. |
| `jn_data` | `json_t *` | **Owned.** A JSON value containing the response payload. If `NULL`, defaults to `json_null()`. |

**Returns**

A new JSON object with the structure `{"result": <int>, "comment": <string>, "schema": <value>, "data": <value>}`. The caller owns the returned object.

**Notes**

All three owned parameters (`jn_comment`, `jn_schema`, `jn_data`) are consumed by the function and must not be used after the call. This function was previously known as `build_webix()`.

---

(command_get_cmd_desc)=
## `command_get_cmd_desc()`

`command_get_cmd_desc()` searches a command table for the descriptor matching a given command name. It extracts the first word from the `command` string and looks it up in `command_table`, checking both the primary name and any aliases defined in each descriptor.

```C
const sdata_desc_t *command_get_cmd_desc(
    const sdata_desc_t *command_table,
    const char *command
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `command_table` | `const sdata_desc_t *` | A null-terminated array of command descriptors to search. |
| `command` | `const char *` | The command string. Only the first word (the command name) is used for matching. |

**Returns**

A pointer to the matching `sdata_desc_t` entry in the command table, or `NULL` if no match is found or the command string is empty.

**Notes**

Aliases have precedence when the descriptor has no `json_fn` command function set. This is the mechanism used to redirect commands as named events.

---

(search_command_desc)=
## `search_command_desc()`

`search_command_desc()` searches for a command descriptor starting in the given GObj and optionally descending into related GObjs depending on the `level` parameter. It first checks the GObj's own command table, then searches deeper if the command is not found locally.

```C
const sdata_desc_t *search_command_desc(
    hgobj gobj,
    const char *command,
    int level,
    hgobj *gobj_found
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance whose command table is searched first. |
| `command` | `const char *` | The command string to search for (first word is used as the command name). |
| `level` | `int` | Search depth: `0` searches only the GObj itself, `1` also searches bottom GObjs (the chain of `gobj_bottom_gobj()`), `2` also searches all direct children. |
| `gobj_found` | `hgobj *` | Output parameter. If not `NULL`, set to the GObj where the command was found, or `NULL` if not found. |

**Returns**

A pointer to the matching `sdata_desc_t` command descriptor, or `NULL` if the command is not found at any searched level.

**Notes**

Uses [`command_get_cmd_desc()`](#command_get_cmd_desc) internally for each GObj's command table lookup.

---

