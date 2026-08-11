---
title: 'JS: Commands, parameters and authorizations'
description: >-
  The SDATA descriptors that declare the commands of a gclass, their
  parameters, their table columns and their authorizations.
---

# Commands, parameters and authorizations

A gclass declares its commands in a table, in the same way that it declares its
attributes. Every descriptor is an `SDataDesc`, and the macros below build one.

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js)

The command table goes to [`gclass_create()`](gclass.md#js_gclass_create), and
[`command_parser()`](events.md#js_command_parser) reads it. A gclass that gives a
command table does not need the `mt_command` method.

```javascript
const command_table = [
    SDATACM(data_type_t.DTP_SCHEMA, "help", 0, pm_help, cmd_help,
        "Command's help"),
    SDATA_END()
];
```

---

## The descriptor

(js_SDataDesc)=
### [`SDataDesc`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L139)

The class that holds one descriptor. Every macro below builds one of these, and
each macro fills a different set of its fields. Build a descriptor with a macro
and never with the constructor: the order of the fields is an internal detail.

(js_SDATA_END)=
### [`SDATA_END()`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L156)

The end of a table. Every table finishes with it.

---

## Commands

(js_SDATACM)=
### [`SDATACM(type, name, alias, items, json_fn, description)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L158)

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
### [`SDATACM2(type, name, flag, alias, items, json_fn, description)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L159)

Declares one command, and adds a `flag`. Use it when the command needs an
authorization flag, such as `SDF_AUTHZ_X`.

---

## Parameters

(js_SDATAPM)=
### [`SDATAPM(type, name, flag, default_value, description)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L160)

Declares one parameter of a command. It takes the same shape as
[`SDATA()`](attrs.md#js_SDATA).

```javascript
const pm_help = [
    SDATAPM(data_type_t.DTP_STRING, "cmd", 0, "", "command about you want help"),
    SDATAPM(data_type_t.DTP_INTEGER, "level", 0, null, "command search level in childs"),
    SDATA_END()
];
```

:::{warning}
Two traps hit a command that an operator calls through the agent, and both come
from the C side of the control plane. A parameter with the name of a field of
the yuno record becomes a filter on that field, and the command answers *"Yuno
not found"*. Give your parameter a name with a prefix. A parameter that is
`SDF_REQUIRED` and `DTP_JSON` or `DTP_INTEGER` arrives without a change of
type, so validate it in the handler.
:::

(js_SDATAPM0)=
### [`SDATAPM0(type, name, flag, authpth, description)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L162)

Declares one parameter with an authorization path, and no default value.

---

## Authorizations

(js_SDATAAUTHZ)=
### [`SDATAAUTHZ(type, name, flag, alias, items, description)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L161)

Declares one authorization of the gclass. The authorization table goes to
[`gclass_create()`](gclass.md#js_gclass_create).

---

## Table columns

(js_SDATADF)=
### [`SDATADF(type, name, flag, header, fillspace, description)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L163)

Declares one column of a table of data. `header` is the title of the column, and
`fillspace` is its part of the width. A schema of a command response uses these
descriptors, so a viewer draws the answer without a schema of its own.
