# Command Parser

Default parser for the control-plane `command` verb. Dispatches a textual command (`help`, `stats`, custom commands…) to the gobj that declared it, handling parameter parsing and authorization.

Source code:

- [`command_parser.h`](https://github.com/artgins/yunetas/blob/7.26.9/kernel/c/gobj-c/src/command_parser.h)
- [`command_parser.c`](https://github.com/artgins/yunetas/blob/7.26.9/kernel/c/gobj-c/src/command_parser.c)

(command_parser)=
## [`command_parser()`](https://github.com/artgins/yunetas/blob/7.26.9/kernel/c/gobj-c/src/command_parser.c#L59)

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

**What a parameter takes**, whatever gives it -- the command line (text), `kw`
(json) or its declared default (text):

| Declared | Takes | Text is read as |
|---|---|---|
| `DTP_STRING` | a string | the text |
| `DTP_DICT` / `DTP_LIST` | a json object / a json array | its json |
| `DTP_INTEGER` / `DTP_REAL` / `DTP_BOOLEAN` | a number or a boolean, never an object or an array | a number with nothing before or after it, in its range; a boolean as `true`/`false`, `yes`/`no`, `on`/`off` or a number |
| `DTP_JSON` | anything | its json, or the text itself as a string (a bare word, a date); a text that starts as json (`{`, `[`, `"`) and does not parse is refused |

**The handler gets the EXACT type** (7.26.10+), so it can read a parameter
with the plain `kw_get_bool()` / `kw_get_int()` / `kw_get_real()` and no
`KW_WILD_NUMBER`:

| Given | to a `DTP_BOOLEAN` | to a `DTP_INTEGER` | to a `DTP_REAL` |
|---|---|---|---|
| `true` / `false` | as it is | `1` / `0` | `1.0` / `0.0` |
| an integer `n` | `true` unless `0` | as it is | `n` as a real |
| a real `d` | `true` unless `0.0` | `d` when it has no decimals (`2.0` -> `2`); with decimals (`2.7`), or out of the 64 bits, **refused** -- as the text `"2.7"` is | as it is |

A number written as text has nothing before or after it (`" 5"` and `"5 "`
are refused), and is in its range: `limit=99999999999999999999` is refused
(`strtoll()` used to saturate it in silence to 2⁶³-1), and so are `nan`,
`inf` and `1e999`.

A `null` is no value (but for a `DTP_JSON`), and so is an empty text given in
`kw` to a number (a form field left empty): the parameter takes its default;
with no default it is ABSENT from the handler's kw -- the `""` used to reach it
--; and an `SDF_REQUIRED` one is refused with the answer *"parameter 'x' is
required"*. On the command line an empty number is `0`, as it always was. The
line has the last word: a value of `kw` that is refused does not count when the
same parameter comes as `key=value` -- and it does not reach the handler under
its name either.

Anything else is refused before the handler runs, with the answer `-1` that
names the parameter, the shape expected and the shape given (never the value:
it may be a secret). A command with a user in `kw` (`__username__`, which
`C_IEVENT_SRV` puts there) came from outside, and the mistake is the peer's: a
WARNING (*"Command parameter of the wrong type"*). From C code it is a broken
contract: an ERROR with a stack.

```C
/*  `node_id` is declared DTP_STRING; a GUI sends a number  */
json_t *resp = gobj_command(gobj_node, "node",
    json_pack("{s:s, s:i, s:s}",
        "topic_name", "departments",
        "node_id", 5,
        "__username__", "ana@example.com"
    ),
    src
);
/*  resp: {"result": -1, "comment":
 *    "<gobj>: command 'node', parameter 'node_id' must be a string, not an integer"}
 *  one WARNING, and the handler never ran                                      */

/*  `force` is declared DTP_BOOLEAN; a GUI sends 1: the handler reads `true`  */
gobj_command(gobj_treedbs, "close-treedb",
    json_pack("{s:s, s:i}", "treedb_name", "treedb_x", "force", 1), src);

/*  `limit` of C_NODE's `nodes` is declared DTP_INTEGER  */
gobj_command(gobj_node, "nodes",
    json_pack("{s:s, s:f}", "topic_name", "departments", "limit", 2.0), src);  /* limit is 2 */
gobj_command(gobj_node, "nodes",
    json_pack("{s:s, s:f}", "topic_name", "departments", "limit", 2.7), src);
/*  -1 "<gobj>: command 'nodes', parameter 'limit' must be an integer, not a real with decimals"  */
```

```bash
ycommand -c 'command-yuno id=<yuno> service=treedbs command=save-schema treedb_name=treedb_x dry_run=yes'   # dry_run is TRUE
ycommand -c 'command-yuno id=<yuno> service=treedbs command=save-schema treedb_name=treedb_x dry_run=maybe' # refused
```

A handler that takes more than one shape on purpose declares `DTP_JSON` and
reads what it gets (`C_NODE`'s `set-link-events set`, the keycloak
`update-idp-user` flags, the `set` of the trace commands of the yuno).

**What a caller must send (BREAKING, 7.26.10+).** Every parameter is typed,
so a caller that relied on the handler's own reading gets `-1` now. Refused:

- a number, a boolean, an object or an array given to a `DTP_STRING`
  (`node_id: 5`);
- given to a `DTP_INTEGER`: a text that is not a whole integer (`play=y`,
  `"5 "`, `" 5"`, `1e3`, a date), a real with decimals (`2.7`), a number past
  64 bits;
- given to a `DTP_REAL`: a text that is not a number, `nan`, `inf`, `1e999`;
- given to a `DTP_BOOLEAN`: a text that is not `true`/`false`, `yes`/`no`,
  `on`/`off` or a number (`maybe`);
- an object or an array given to a number or a boolean, a non-object to a
  `DTP_DICT`, a non-array to a `DTP_LIST`;
- a text that starts as json and does not parse, given to a `DTP_JSON`.

Before, such a value reached the handler, which logged an ERROR or read it as
something else. Look at the command's `help` for the type of each parameter:
**the same name can have another type in another command** -- `from_t` /
`to_t` are a `DTP_JSON` (a date or a number) in `C_TRANGER`'s `open-list` and a
`DTP_INTEGER` in its `open-iterator`; `options` is a string, an integer or a
json; `set` is a json in the yuno's trace commands and a boolean or a json
elsewhere.

**Authorization comes first.** The per-command gate of the parser
(`SDF_AUTHZ_X`, when the yuno sets `enable_command_authz`) is asked BEFORE the
parameters are read, so a caller without the permission gets `-403`, never the
diagnostic of a parameter. The checker is asked for the command's NAME (the
`command` of `__execute_command__`), and a denial logs and answers the name:
up to 7.26.9 it was given the whole line, with its parameters -- a password
among them -- and logged it. A permission that a HANDLER asks itself
(`C_TREEDB`'s `create`, `C_NODE`'s `read`, ...) is asked by the handler, so
after the typing: the parser does not know it.

**Who is a peer.** A command is a peer's when its kw carries `__username__`.
`C_IEVENT_SRV` writes it into every command that comes in from outside, over
whatever the client put there, so a client cannot choose it. Two things follow,
and they are the model, not a hole:

- a command that comes in by a `C_IEVENT_CLI` link -- this yuno is the CLIENT
  of another one, and the other side sends it a command -- is dispatched with
  the kw the server sent. If the server is not an agent and does not stamp
  `__username__`, the command is a C caller's here: trusted, its mistakes are
  ERRORs, and the peer checks of `C_TREEDB` / `C_TRANGER` do not apply. A yuno
  connects as a client only to a yuno it trusts;
- a rule of "the same user" (the `force` of `C_TREEDB`'s `close-treedb` on a
  treedb a peer opened) compares that name: everything that comes in under one
  identity of an agent is one user. And a treedb a peer opened, once it is
  closed, is anybody's with the permission: another user can delete it or open
  it again.

Up to 7.26.9 a value of another shape went to the handler as it came, and its
`kw_get_*()` logged the caller's mistake as an ERROR with a stack -- or read it
as something else: `C_TREEDB`'s `save-schema` read `"treedb_name": 5` as no
treedb named, and saved every treedb open; `dry_run=yes` was `atoi()`'d to
FALSE, and the save was real. A `SDF_REQUIRED` parameter given in `kw` was not
converted at all.

---

(gobj_build_cmds_doc)=
## [`gobj_build_cmds_doc()`](https://github.com/artgins/yunetas/blob/7.26.9/kernel/c/gobj-c/src/command_parser.c#L1214)

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
## [`build_command_response()`](https://github.com/artgins/yunetas/blob/7.26.9/kernel/c/gobj-c/src/command_parser.c#L1269)

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
## [`command_get_cmd_desc()`](https://github.com/artgins/yunetas/blob/7.26.9/kernel/c/gobj-c/src/command_parser.c#L202)

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

(command_parameter_from_text)=
## [`command_parameter_from_text()`](https://github.com/artgins/yunetas/blob/7.26.9/kernel/c/gobj-c/src/command_parser.c)

A value of `type` written as TEXT, read exactly as [`command_parser()`](#command_parser)
reads a parameter written on the command line: a number with nothing before or
after it and in its range, a boolean as `true`/`false`, `yes`/`no`, `on`/`off`
or a number, an object or a list as its json, a `DTP_JSON` as its json or else
the text itself; an empty text is `0` or `false`. It is for a command whose
value takes the type of something else: the yuno's `write-attr value=...`
reads the value as the type of the ATTRIBUTE.

```C
json_t *command_parameter_from_text(
    data_type_t type,
    const char *text,
    const char **refused
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `type` | `data_type_t` | The type to read the text as: `DTP_STRING`, `DTP_INTEGER`, `DTP_REAL`, `DTP_BOOLEAN`, `DTP_DICT`, `DTP_LIST` or `DTP_JSON`. |
| `text` | `const char *` | The text. `NULL` is an empty text. |
| `refused` | `const char **` | Out, may be `NULL`: why the text was refused, a short phrase (`"a text that does not read as one"`, `"a number out of its range"`), never the text -- it may be a secret. `NULL` when it was read. |

**Returns**

A NEW reference to the value, of the exact json type of `type`; `NULL` when the
text does not read as one.

**Example**

```C
const char *why = NULL;
json_t *jn = command_parameter_from_text(DTP_BOOLEAN, "yes", &why);    /* true */
JSON_DECREF(jn)
jn = command_parameter_from_text(DTP_INTEGER, "12abc", &why);
/*  NULL, why: "a text that does not read as one" -- strtoll() read 12  */
jn = command_parameter_from_text(DTP_INTEGER, "99999999999999999999", &why);
/*  NULL, why: "a number out of its range" -- strtoll() saturated it  */
```

**Notes**

`write-attr attribute=<a> value=<text>` of `C_YUNO` reads `value` this way for
a boolean, integer or real attribute: `atoi()` and `strtoll()` wrote `"yes"`
as FALSE, `"abc"` as 0 and `"12abc"` as 12. A text that does not read as the
attribute's type is refused, `-1`, and nothing is written.

---

(command_mask_secret_kw)=
## [`command_mask_secret_kw()`](https://github.com/artgins/yunetas/blob/7.26.9/kernel/c/gobj-c/src/command_parser.c#L403)

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
## [`command_mask_secret_line()`](https://github.com/artgins/yunetas/blob/7.26.9/kernel/c/gobj-c/src/command_parser.c#L424)

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
A value longer than 1 KB, keyed or positional, is shown as `<N bytes>`: the
`content64` of an `install-binary` is tens of MB of base64, nothing a trace
can show (up to 7.26.5 it overflowed the 64 KB buffer of the line and logged
*"MAXIMUM SPACE REACHED"*). The `commands` (and `machine`) trace of `gobj_command()` prints the command
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

line = command_mask_secret_line(agent,  // a value of 46 MB of base64
    "install-binary id=emailsender content64=f0VMRgIBAQAAAA..."
);                                      // "install-binary id=emailsender content64=<45984395 bytes>"

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

That masking holds because a required secret is always the LAST required
parameter: [`gclass_create()`](#gclass_create) refuses a command table where
a required secret (`SDF_SECRET`, or a secret's name) is followed by another
required parameter. There a secret written with blanks would spill into the
next one: `login correct horse battery` gives `correct` to the secret,
`horse` to the next parameter -- shown in the traces and its errors -- and
echoes `battery`. Declare a second required parameter BEFORE the secret, or
make it optional:

```C
PRIVATE sdata_desc_t pm_login[] = {
/*-PM----type-----------name------------flag------------------------default-description--*/
SDATAPM (DTP_STRING,    "user",         SDF_REQUIRED,               0,      "User"),
SDATAPM (DTP_STRING,    "password",     SDF_REQUIRED|SDF_SECRET,    0,      "Password, the last required one"),
SDATAPM (DTP_STRING,    "note",         0,                          "",     "Optional, after it"),
SDATA_END()
};
```

A value opened with a quote and never closed (`password='abc`) refuses the
command: *"command 'set-password', parameter 'password': value with no
closing quote"*. Up to 7.25.20 the parameter was dropped and the command ran
without it, with no log.

---

(search_command_desc)=
## [`search_command_desc()`](https://github.com/artgins/yunetas/blob/7.26.9/kernel/c/gobj-c/src/command_parser.c#L1028)

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

