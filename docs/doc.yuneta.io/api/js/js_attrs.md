---
title: 'JS: Attributes'
description: >-
  Declaring the attribute schema of a gclass with SDATA, and reading and
  writing the attributes of a gobj.
---

# Attributes

A gclass declares its attributes in a table. Each attribute has a type, a name,
a set of flags, a default value and a description.

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js)

```javascript
import { SDATA, SDATA_END, data_type_t, sdata_flag_t } from "@yuneta/gobj-js";

const attrs_table = [
    SDATA(data_type_t.DTP_STRING,  "url",       sdata_flag_t.SDF_RD,      "", "Server URL"),
    SDATA(data_type_t.DTP_INTEGER, "timeout",   sdata_flag_t.SDF_RD,       0, "Timeout ms"),
    SDATA(data_type_t.DTP_BOOLEAN, "connected", sdata_flag_t.SDF_RD,   false, "Connection state"),
    SDATA(data_type_t.DTP_STRING,  "saved_key", sdata_flag_t.SDF_PERSIST, "", "Persisted value"),
    SDATA_END()
];
```

:::{warning}
A gclass with no attributes still needs a table that holds
[`SDATA_END()`](commands.md#js_SDATA_END). An empty value there passes the
registration, and the first creation of an instance fails.
:::

---

## Declare

(js_SDATA)=
### [`SDATA(type, name, flag, default_value, description)`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L182)

Declares one attribute. The table finishes with
[`SDATA_END()`](commands.md#js_SDATA_END).

(js_data_type_t)=
### [`data_type_t`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L109)

The types of an attribute.

| Type | Description |
|---|---|
| `DTP_STRING` | A string. |
| `DTP_BOOLEAN` | A boolean. |
| `DTP_INTEGER` | An integer. |
| `DTP_REAL` | A real number. |
| `DTP_LIST` | A list. |
| `DTP_DICT` | An object. |
| `DTP_JSON` | Any JSON value. |
| `DTP_POINTER` | A value that is not JSON, such as a gobj or a DOM node. |

(js_sdata_flag_t)=
### [`sdata_flag_t`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L133)

The flags of an attribute. Combine them with the bit-or operator.

| Flag | Description |
|---|---|
| `SDF_NOTACCESS` | Not reachable from outside. |
| `SDF_RD` | A reader can read it. |
| `SDF_WR` | A writer can write it. |
| `SDF_REQUIRED` | The creation must give a value. |
| `SDF_PERSIST` | The framework saves it and loads it again. |
| `SDF_VOLATIL` | [`gobj_reset_volatil_attrs()`](#js_gobj_reset_volatil_attrs) clears it. |
| `SDF_RESOURCE` | A resource. |
| `SDF_PKEY` | The primary key of a record. |
| `SDF_DEPRECATED` | Kept for the old callers. |
| `SDF_WILD_CMD` | A command that accepts a free form. |
| `SDF_STATS` | A statistic. |
| `SDF_FKEY` | A foreign key of a record. |
| `SDF_RSTATS` | A statistic that resets when a reader reads it. |
| `SDF_PSTATS` | A statistic that persists. |
| `SDF_AUTHZ_R` | The read needs an authorization. |
| `SDF_AUTHZ_W` | The write needs an authorization. |
| `SDF_AUTHZ_X` | The execution needs an authorization. |
| `SDF_AUTHZ_P` | The subscription needs an authorization. |
| `SDF_AUTHZ_S` | The statistics need an authorization. |
| `SDF_AUTHZ_RS` | The reset of the statistics needs an authorization. |
| `SDF_SECRET` | A secret (a password, a token). It is read and saved as usual, and a log never shows its value (7.26.22+). |

---

## Read

:::{note}
A `path` accepts the back-tick to reach an attribute of a child gobj:
``"child`subattr"``.
:::

(js_gobj_has_attr)=
### [`gobj_has_attr(gobj, name)`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L3618)

Tells if a gobj has an attribute with that name.

(js_gobj_read_attr)=
### [`gobj_read_attr(gobj, name, src)`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L3629)

Reads an attribute of any type. `src` names the gobj that asks, for the
authorization and for the log.

(js_gobj_read_attrs)=
### [`gobj_read_attrs(gobj, include_flag, src)`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L3652)

Reads every attribute whose flags match `include_flag`, as one object.

(js_gobj_read_bool_attr)=
### [`gobj_read_bool_attr(gobj, name)`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L3688)

Reads a boolean attribute.

(js_gobj_read_integer_attr)=
### [`gobj_read_integer_attr(gobj, name)`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L3719)

Reads an integer attribute.

(js_gobj_read_str_attr)=
### [`gobj_read_str_attr(gobj, name)`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L3742)

Reads a string attribute.

(js_gobj_read_pointer_attr)=
### [`gobj_read_pointer_attr(gobj, name)`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L3765)

Reads an attribute that holds a value which is not JSON. The `subscriber`
attribute of the subscription model uses it.

(js_gobj_hsdata)=
### [`gobj_hsdata(gobj)`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L1391)

Gives the whole store of the attributes of a gobj. It is an internal handle, and
the functions above are the way to read an attribute.

---

## Write

(js_gobj_write_attr)=
### [`gobj_write_attr(gobj, path, value, src)`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L3784)

Writes an attribute of any type. The value is read as the type of the
attribute, as in C and as [`gobj_write_attrs()`](#js_gobj_write_attrs) reads it
(7.26.22+). A refused value is not written, and the function returns a
negative number. `undefined` for a boolean, integer, real, list or dict
attribute is refused: this is an explicit write of nothing. Up to 7.26.21 the
value was written as it was given.

(js_gobj_write_attrs)=
### [`gobj_write_attrs(gobj, kw, include_flag, src)`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L3945)

Writes every attribute of `kw` whose flags match `include_flag`.

Each value is read as the type of its attribute, as in C (7.26.22+). The kw of
[`gobj_create()`](lifecycle.md#js_gobj_create) and the loaded persistent attributes
are read in the same way. The reader is
[`command_parameter_from_json()`](#js_command_parameter_from_json):

- A string for a `DTP_BOOLEAN`, `DTP_INTEGER`, `DTP_REAL`, `DTP_LIST` or
  `DTP_DICT` attribute is read from its text: `"yes"` is `true`, `"42"` is
  `42`, `"[1, 2]"` is a list.
- A number or a boolean is read as the exact type: `3` for a real is `3`, and
  `true` for an integer is `1`.
- A key with the value `undefined` is an absent key. Nothing is written.

If a value is not of the type (`"12abc"`, `2.7` for an integer, `"maybe"`),
the attribute keeps its value. One `log_error` names the attribute, its type,
the cause and the value. The value is masked and cut to 64 characters, and the
value of a `SDF_SECRET` attribute is never shown. The function returns a
negative number.

```javascript
gobj_write_attrs(gobj, {timeout: "1000", ratio: 3}, -1, gobj);   // 1000 and 3
if(gobj_write_attrs(gobj, {timeout: "10s"}, -1, gobj) < 0) {
    // Error already logged: timeout keeps 1000
}
```

Up to 7.26.21, `parseInt("12abc")` wrote `12`, `Number("")` wrote `0`, and
`"maybe"` wrote `false`.

(js_command_parameter_from_json)=
### `command_parameter_from_json(type, value)`

Reads a value of `type` that is given as JSON, as the C command parser reads a
parameter in a kw (`command_parameter_from_json()` of C, 7.26.22+). The
grammar is the same in C and in JS. The test table
`tests/fixtures/attr_types_parity.json` is read by the tests of the two
kernels.

- A string is read as [`command_parameter_from_text()`](#js_command_parameter_from_text)
  reads a text. A string with a NUL in it is refused.
- A number or a boolean is read as the exact type. `2.7` for an integer is
  refused.
- An object or a list must have the shape of the type.

It returns `{value, refused}`. If `value` is `undefined`, `refused` gives the
cause, and it never contains the value. If `refused` is `null` too, there is
no value: a `null`, `undefined`, or an empty text for a number.

```javascript
command_parameter_from_json(data_type_t.DTP_INTEGER, "42");   // {value: 42, refused: null}
command_parameter_from_json(data_type_t.DTP_INTEGER, 2.7);    // {value: undefined, refused: "a real with decimals"}
command_parameter_from_json(data_type_t.DTP_BOOLEAN, "");     // {value: undefined, refused: null}
```

(js_command_parameter_from_text)=
### `command_parameter_from_text(type, text)`

Reads a value of `type` that is written as text, as the C command parser reads
a parameter on the command line (7.26.22+):

- A number has the JSON grammar, the whole text, and is in its range. `"007"`,
  `"+5"`, `".5"` and `"nan"` are refused.
- A boolean is `true`/`false`, `yes`/`no`, `on`/`off`, or an integer.
- An object or a list is read from its JSON.
- A `DTP_JSON` is read from its JSON. If the text is not JSON, it is the text
  itself.
- An empty text is `0` or `false`.

It returns `{value, refused}`, as [`command_parameter_from_json()`](#js_command_parameter_from_json).

```javascript
command_parameter_from_text(data_type_t.DTP_BOOLEAN, "yes");    // {value: true, refused: null}
command_parameter_from_text(data_type_t.DTP_INTEGER, "12abc");  // {value: undefined, refused: "a text that does not read as one"}
```

(js_gobj_write_bool_attr)=
### [`gobj_write_bool_attr(gobj, name, value)`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L3970)

Writes a boolean attribute. The value is read as a boolean by the grammar of
[`command_parameter_from_json()`](#js_command_parameter_from_json) (7.26.22+):
`"yes"` and `1` are `true`. A value that is not a boolean (`"maybe"`, an
object, `undefined`) is refused: one `log_error`, a negative return, and the
attribute keeps its value. Up to 7.26.21, `"yes"` was written `false`.

(js_gobj_write_integer_attr)=
### [`gobj_write_integer_attr(gobj, name, value)`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L4002)

Writes an integer attribute. The value is read as an integer, as
[`gobj_write_bool_attr()`](#js_gobj_write_bool_attr) reads a boolean (7.26.22+):
`"42"` is `42`, and `2.5`, `NaN`, `"12abc"` and `undefined` are refused. Up to
7.26.21 `parseInt()` wrote `2` for `2.5` and `12` for `"12abc"`. A time for
`set_timeout()` is rounded to whole milliseconds before it is written.

(js_gobj_write_str_attr)=
### [`gobj_write_str_attr(gobj, name, value)`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L4034)

Writes a string attribute. `null` and `undefined` are written as `""`, and
any other value that is not a string as its JSON text (7.26.22+). Up to
7.26.21 they were written as the texts `"null"` and `"undefined"`.

```javascript
gobj_write_integer_attr(gobj, "timeout", "1000");       // 1000
gobj_write_str_attr(gobj, "username", kw.username);     // "" when kw has none
```

(js_gobj_reset_volatil_attrs)=
### [`gobj_reset_volatil_attrs(gobj)`](https://github.com/artgins/gobj-js/blob/7.26.23/src/gobj.js#L3933)

Writes the default value in every attribute that carries `SDF_VOLATIL`.

---

An attribute with `SDF_PERSIST` goes to the store through the callbacks that
[`gobj_start_up()`](bootstrap.md#js_gobj_start_up) receives. See
[Persistence](persistence.md).
