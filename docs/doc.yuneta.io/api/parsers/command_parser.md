# Command Parser

Default parser for the control-plane `command` verb. Dispatches a textual command (`help`, `stats`, custom commands…) to the gobj that declared it, handling parameter parsing and authorization.

Source code:

- [`command_parser.h`](https://github.com/artgins/yunetas/blob/7.25.21/kernel/c/gobj-c/src/command_parser.h)
- [`command_parser.c`](https://github.com/artgins/yunetas/blob/7.25.21/kernel/c/gobj-c/src/command_parser.c)

(command_parser)=
## [`command_parser()`](https://github.com/artgins/yunetas/blob/7.25.21/kernel/c/gobj-c/src/command_parser.c#L54)

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

The handler does not get `kw`: it gets a new kw, owned by the handler, with
the parameters of the command and then every key of `kw` that is not a
parameter ([`kw_update_missing()`](#kw_update_missing)). A binary field of
`kw` (a `gbuffer`) is shared with a reference of its own: the handler releases
its kw with `KW_DECREF`, and `command_parser()` releases `kw`. A handler that
keeps the buffer takes it out of its kw, and the reference it takes is its
own:

```C
PRIVATE json_t *cmd_upload(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_gbuf = kw_get_dict_value(gobj, kw, "gbuffer", 0, KW_EXTRACT);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)json_integer_value(jn_gbuf);
    JSON_DECREF(jn_gbuf)

    // ... gbuf is yours: hand it on, or GBUFFER_DECREF(gbuf) ...

    return msg_iev_build_response(gobj, 0, 0, 0, 0, kw);  // releases kw
}
```

Up to 7.25.4 the copy took no reference, and the gbuffer was released once
too often (*"BAD gbuf_decref()"*).

---

(gobj_build_cmds_doc)=
## [`gobj_build_cmds_doc()`](https://github.com/artgins/yunetas/blob/7.25.21/kernel/c/gobj-c/src/command_parser.c#L1173)

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
## [`build_command_response()`](https://github.com/artgins/yunetas/blob/7.25.21/kernel/c/gobj-c/src/command_parser.c#L1228)

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
## [`command_get_cmd_desc()`](https://github.com/artgins/yunetas/blob/7.25.21/kernel/c/gobj-c/src/command_parser.c#L197)

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

(command_mask_secret_kw)=
## [`command_mask_secret_kw()`](https://github.com/artgins/yunetas/blob/7.25.21/kernel/c/gobj-c/src/command_parser.c#L395)

The kw of a command as a trace shows it. A value is `"********"` when its key
is a parameter the command table of `gobj` declares with `SDF_SECRET` (by
name or alias), or when the key has a secret's NAME
([`is_secret_name()`](#is_secret_name): `password`, `token`, `api_key`, ...) --
at any depth of the kw. The name rule is what covers a key the table cannot
know: the free keys of a `SDF_WILD_CMD` command, which forwards them to a table
somewhere else (the agent's `command-yuno ... password=X`, whose `password`
belongs to the remote yuno's command). The rest is
[`json_mask_secrets()`](#json_mask_secrets): a string with a secret
`name=value` in it (the `command` of `command-yuno`) is masked inside, and so
is the `value` of a write-attr whose `attribute` names a secret. A secret is masked whatever its
json type (`"password": 1234` too); only an absent one or an empty string
shows as it is, so "not set" still shows. The kernel uses it for the
`"command kw"` trace of `gobj_command()` and the `"expanded_command: kw_cmd"`
trace of [`command_parser()`](#command_parser) (both under `ev_kw`).

```C
json_t *command_mask_secret_kw(
    hgobj gobj,
    const char *command,    // "name [parameters]"
    json_t *kw              // not owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The gobj whose command table describes the command. |
| `command` | `const char *` | The command; only its first word is used to find the descriptor. |
| `kw` | `json_t *` | The kw of the command. Not owned. |

**Returns**

A NEW reference, to decref: a masked copy, or `kw` itself when nothing in it
is secret. `NULL` for a `NULL` kw.

**Example**

```C
/*
 *  The parameter is declared secret in the command's schema
 */
PRIVATE sdata_desc_t pm_set_email_user[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,              0,          "User name"),
SDATAPM (DTP_STRING,    "password",     SDF_SECRET,     0,          "Password"),
SDATA_END()
};

json_t *kw_shown = command_mask_secret_kw(gobj, "set-email-user", kw);
gobj_trace_json(gobj, kw_shown, "command kw");  // "password": "********"
JSON_DECREF(kw_shown)

/*
 *  A wild command: the free keys are masked by name
 */
SDATACM2 (DTP_SCHEMA,   "command-yuno",     SDF_WILD_CMD,   0,  pm_command_yuno, cmd_command_yuno, "Command to yuno"),

kw_shown = command_mask_secret_kw(gobj, "command-yuno",
    kw  // {"id": "x", "command": "set-user-pwd", "password": "hunter2"}
);      // {"id": "x", "command": "set-user-pwd", "password": "********"}
```

---

(command_mask_secret_line)=
## [`command_mask_secret_line()`](https://github.com/artgins/yunetas/blob/7.25.21/kernel/c/gobj-c/src/command_parser.c#L415)

The command line as a trace shows it: the value of every secret parameter is
`********` -- `SDF_SECRET` in the command table of `gobj`, or with a secret's
name ([`is_secret_name()`](#is_secret_name)), as for
[`command_mask_secret_kw()`](#command_mask_secret_kw) -- given as `key=value`
or as one of the leading required parameters written without key. A
positional value with a `=` in it (`abc==`, `'pa=ss'`) is that parameter's
value, unless what is before the `=` is a key of the command or a secret's
name: then it is the first `key=value`. In a `SDF_WILD_CMD` command a value
with a `=` is a command line going on, shown `key='...'` masked by names. The
whole line then goes through [`mask_secrets_inline()`](#mask_secrets_inline):
`write-attr attribute=password value=X` shows `value=********`. What
cannot be parsed as a parameter is shown as `<...>`, never dropped silently.
The `commands` (and `machine`) trace of `gobj_command()` prints the command
this way.

```C
char *command_mask_secret_line(
    hgobj gobj,
    const char *command     // "name [parameters]"
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The gobj whose command table describes the command. |
| `command` | `const char *` | The command line. |

**Returns**

A `gbmem` string, to `GBMEM_FREE`: the line as it was parsed (quotes and
blanks are not kept).

**Example**

```C
char *line = command_mask_secret_line(gobj, "set-email-user username=bob password=hunter2");
gobj_trace_msg(gobj, "cmd: %s", line);  // "set-email-user username=bob password=********"
GBMEM_FREE(line)

line = command_mask_secret_line(gobj,   // "password" is SDF_REQUIRED|SDF_SECRET
    "set-password-pos 'q=hunter2' note=visible"
);                                      // "set-password-pos ******** note=visible"

line = command_mask_secret_line(agent,  // command-yuno is SDF_WILD_CMD
    "command-yuno id=x command=\"set-user-pwd password=hunter2\""
);                                      // "command-yuno id=x command='set-user-pwd password=********'"
```

The errors the parser answers for a malformed line mask what they echo: a
secret `name=value` in the extra text, and all of it when it is the value of a
secret parameter written with a blank after its `=`, or the rest of a
positional secret written with blanks. `set-password password=
hunter2` is refused with *"command 'set-password' with extra parameters:
'<...>'"*, and so is `set-password-pos correct horse battery` (a required
`SDF_SECRET` parameter given without its key: it takes `correct`, and up to
7.25.21 the answer showed `'horse battery'`); `list-yunos foo` with *"...
extra parameters: 'foo'"*.

A value opened with a quote and never closed (`password='abc`) refuses the
command: *"command 'set-password', parameter 'password': value with no
closing quote"*. Up to 7.25.20 the parameter was dropped and the command ran
without it, with no log.

---

(search_command_desc)=
## [`search_command_desc()`](https://github.com/artgins/yunetas/blob/7.25.21/kernel/c/gobj-c/src/command_parser.c#L987)

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

