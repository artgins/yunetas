---
title: 'JS: Commands, parameters and authorizations'
description: >-
  The SDATA descriptors that declare the commands of a gclass, their
  parameters, their table columns and their authorizations.
---

# Commands, parameters and authorizations

A gclass declares its commands in a table, in the same way that it declares its
attributes. Every descriptor is an `SDataDesc`, and the macros below build one.

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.29.4/src/gobj.js)

The command table goes to [`gclass_create()`](js_gclass.md#js_gclass_create):

```javascript
const command_table = [
    SDATACM(data_type_t.DTP_SCHEMA, "help", 0, pm_help, cmd_help,
        "Command's help"),
    SDATA_END()
];
```

[`gobj_command()`](events.md#js_gobj_command) hands the command and its `kw`,
unchanged, to the first of these that the gclass has:

1. its own `mt_command_parser` method;
2. when the gclass has a command table, the global parser given to
   [`gobj_start_up()`](bootstrap.md#js_gobj_start_up)
   (`global_command_parser_fn`).

A gclass with a table and no global parser gets `-1` *"\<gobj>: global
command parser not available"*. A gclass with no table and no
`mt_command_parser` gets `-1` *"\<gobj>: command table not available"*: the
global parser is never asked for it. The
[`command_parser()`](events.md#js_command_parser) of gobj-js is an empty stub:
it reads no table, types no parameter and returns `undefined`. So a gclass
that must answer its commands gives its own `mt_command_parser`, as `C_YUNO`
does for its trace commands:

```javascript
function mt_command_parser(gobj, command, kw, src)
{
    switch(command) {
        case "get-global-trace":
            return build_command_response(gobj, 0, null, null, gobj_get_global_trace_level());
        default:
            return build_command_response(gobj, -1, `${gobj_yuno_role()}^${gobj_yuno_name()}: command not available: ${command}`, null, null);
    }
}

const gmt = {
    mt_command_parser: mt_command_parser,
};
```

---

## The descriptor

(js_SDataDesc)=
### [`SDataDesc`](https://github.com/artgins/gobj-js/blob/7.29.4/src/gobj.js#L165)

The class that holds one descriptor. Every macro below builds one of these, and
each macro fills a different set of its fields. Build a descriptor with a macro
and never with the constructor: the order of the fields is an internal detail.

(js_SDATA_END)=
### [`SDATA_END()`](https://github.com/artgins/gobj-js/blob/7.29.4/src/gobj.js#L182)

The end of a table. Every table finishes with it.

---

## Commands

(js_SDATACM)=
### [`SDATACM(type, name, alias, items, json_fn, description)`](https://github.com/artgins/gobj-js/blob/7.29.4/src/gobj.js#L184)

Declares one command.

| Key | Description |
|---|---|
| `type` | `DTP_SCHEMA` for a command. |
| `name` | The name that the operator writes. |
| `alias` | A list of other names for the same command. |
| `items` | The table of the parameters, built with `SDATAPM`. |
| `json_fn` | The function that runs the command. |
| `description` | The help line of the command. |

(js_SDATACM2)=
### [`SDATACM2(type, name, flag, alias, items, json_fn, description)`](https://github.com/artgins/gobj-js/blob/7.29.4/src/gobj.js#L185)

Declares one command, and adds a `flag`. Use it when the command needs an
authorization flag, such as `SDF_AUTHZ_X`.

---

## Parameters

(js_SDATAPM)=
### [`SDATAPM(type, name, flag, default_value, description)`](https://github.com/artgins/gobj-js/blob/7.29.4/src/gobj.js#L186)

Declares one parameter of a command. It takes the same shape as
[`SDATA()`](js_attrs.md#js_SDATA).

```javascript
const pm_help = [
    SDATAPM(data_type_t.DTP_STRING, "cmd", 0, "", "command about you want help"),
    SDATAPM(data_type_t.DTP_INTEGER, "level", 0, null, "command search level in childs"),
    SDATA_END()
];
```

:::{warning}
**gobj-js does not type the parameters of a command.** A JS command gets
its `kw` as the caller wrote it (see above), so its handler checks every type
it needs. The typing below is the C kernel's: it happens where a C yuno parses
a command, and that includes a command that a SPA sends to a C yuno. Then the
`SDATAPM` table of the C yuno's command decides, not a table in the SPA.

Two things to know about a command of a C yuno that an operator calls through
the agent. A parameter with the name of a field of the yuno record becomes a
filter on that field, and the command answers *"Yuno not found"*. Give your
parameter a name with a prefix. And the C parser types every parameter,
`SDF_REQUIRED` or not, since SDK 7.27.0: a `DTP_INTEGER`, `DTP_REAL` or
`DTP_BOOLEAN` given as text is read as one (`"5"` is `5`), and a
`DTP_STRING`, `DTP_DICT`, `DTP_LIST` or number of another shape is refused
before the handler (see [`command_parser()`](#command_parser)). Only a
`DTP_JSON` parameter arrives in any shape, so validate that one in the
handler. Up to SDK 7.26.9 a `SDF_REQUIRED` parameter given in the kw arrived
without a change of type.

```C
/*  the parameters of a command of a C yuno  */
SDATAPM (DTP_INTEGER,   "limit",    0,  "100",  "rows of a page"),                      // "50" arrives as 50
SDATAPM (DTP_JSON,      "set",      0,  "",     "a number, a boolean or their text"),   // any shape: check it
```

```javascript
// a SPA sends the command to the C yuno through its C_IEVENT_CLI:
// the C parser reads "50" as 50 before the C handler runs
gobj_command(gobj_remote_yuno, "list-rows", {service: "my_service", limit: "50"}, gobj);
```
:::

(js_SDATAPM0)=
### [`SDATAPM0(type, name, flag, authpth, description)`](https://github.com/artgins/gobj-js/blob/7.29.4/src/gobj.js#L188)

Declares one parameter with an authorization path, and no default value.

---

## Authorizations

(js_SDATAAUTHZ)=
### [`SDATAAUTHZ(type, name, flag, alias, items, description)`](https://github.com/artgins/gobj-js/blob/7.29.4/src/gobj.js#L187)

Declares one authorization of the gclass. The authorization table goes to
[`gclass_create()`](js_gclass.md#js_gclass_create).

---

## Table columns

(js_SDATADF)=
### [`SDATADF(type, name, flag, header, fillspace, description)`](https://github.com/artgins/gobj-js/blob/7.29.4/src/gobj.js#L189)

Declares one column of a table of data. `header` is the title of the column, and
`fillspace` is its part of the width. A schema of a command response uses these
descriptors, so a viewer draws the answer without a schema of its own.
