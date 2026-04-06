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

*Description pending — signature extracted from header.*

```C
json_t *build_command_response(
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment,
    json_t *jn_schema,
    json_t *jn_data
);
```

---

(command_get_cmd_desc)=
## `command_get_cmd_desc()`

*Description pending — signature extracted from header.*

```C
const sdata_desc_t *command_get_cmd_desc(
    const sdata_desc_t *command_table,
    const char *command
);
```

---

(search_command_desc)=
## `search_command_desc()`

*Description pending — signature extracted from header.*

```C
const sdata_desc_t *search_command_desc(
    hgobj gobj,
    const char *command,
    int level,
    hgobj *gobj_found
);
```

---

