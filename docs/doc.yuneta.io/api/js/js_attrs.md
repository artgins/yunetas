---
title: 'JS: Attributes'
description: >-
  Declaring the attribute schema of a gclass with SDATA, and reading and
  writing the attributes of a gobj.
---

# Attributes

A gclass declares its attributes in a table. Each attribute has a type, a name,
a set of flags, a default value and a description.

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js)

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
### [`SDATA(type, name, flag, default_value, description)`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L183)

Declares one attribute. The table finishes with
[`SDATA_END()`](commands.md#js_SDATA_END).

(js_data_type_t)=
### [`data_type_t`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L110)

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
### [`sdata_flag_t`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L134)

The flags of an attribute. Combine them with the bit-or operator.

| Flag | Description |
|---|---|
| `SDF_NOTACCESS` | Not reachable from outside. |
| `SDF_RD` | A reader can read it. |
| `SDF_WR` | A writer can write it. |
| `SDF_REQUIRED` | The attribute must have a value before the start: [`gobj_start()`](lifecycle.md#js_gobj_start) refuses an empty one (gobj-js 7.26.28). |
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
### [`gobj_has_attr(gobj, name)`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L3998)

Tells if a gobj has an attribute with that name.

(js_gobj_read_attr)=
### [`gobj_read_attr(gobj, name, src)`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L4009)

Reads an attribute of any type. `src` names the gobj that asks, for the
authorization and for the log.

(js_gobj_read_attrs)=
### [`gobj_read_attrs(gobj, include_flag, src)`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L4032)

Reads every attribute whose flags match `include_flag`, as one object.

(js_gobj_read_bool_attr)=
### [`gobj_read_bool_attr(gobj, name)`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L4068)

Reads a boolean attribute.

(js_gobj_read_integer_attr)=
### [`gobj_read_integer_attr(gobj, name)`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L4099)

Reads an integer attribute.

(js_gobj_read_str_attr)=
### [`gobj_read_str_attr(gobj, name)`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L4122)

Reads a string attribute.

(js_gobj_read_pointer_attr)=
### [`gobj_read_pointer_attr(gobj, name)`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L4145)

Reads an attribute that holds a value which is not JSON. The `subscriber`
attribute of the subscription model uses it.

(js_gobj_hsdata)=
### [`gobj_hsdata(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L1696)

Gives the whole store of the attributes of a gobj. It is an internal handle, and
the functions above are the way to read an attribute.

---

## Write

(js_gobj_write_attr)=
### [`gobj_write_attr(gobj, path, value, src)`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L4164)

Writes an attribute of any type. The value is read as the type of the
attribute, as in C and as [`gobj_write_attrs()`](#js_gobj_write_attrs) reads it
(7.26.22+). A refused value is not written, the function returns a negative
number, and the attribute keeps its value. `undefined`, `null` and `""` for a
boolean, integer, real, list or dict attribute are refused: this is an
explicit write of nothing. Up to 7.26.21 the value was written as it was given
(BREAKING in 7.26.22).

```javascript
gobj_write_attr(gobj, "ratio", 1.5);
gobj_write_attr(gobj, "ratio", "");     // -1, one ERROR, "ratio" is still 1.5
gobj_write_attr(gobj, "ratio", "2.5");  // 2.5
```

(js_gobj_write_attrs)=
### [`gobj_write_attrs(gobj, kw, include_flag, src)`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L4331)

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
- An object or a list must have the shape of the type, and it must be JSON
  all the way down (7.26.25): no cycle, `BigInt`, `Map`, `Set`, `Date`, typed
  array or instance of a class anywhere inside. Its JSON text must also give
  the same value (7.26.26), so these are refused too: an object with a
  `toJSON()`, `NaN` or `Infinity` inside, a function, `undefined`, a symbol or a
  hole inside an array (`[1, , 3]`), and a value nested deeper than 2048
  levels. Every version before 7.26.26 accepted them. A getter or a
  `Proxy` trap that throws gives a refusal, not an exception. A key whose value
  is `undefined` or a function is an absent key. A plain object of another
  realm (a `vm` context, an iframe) is a plain object. The cause is
  `"a value with no json text"`.
- A JSON text that the C parser refuses is refused in JS too (7.26.24),
  although `JSON.parse()` reads it: an escaped NUL (`"\u0000"`), a lone
  surrogate (`"\ud800"`), an integer out of int64, a real that overflows
  (`1e309`). For a `DTP_JSON` such a text stays the text: `"1e309"` is the
  string `"1e309"`, not `Infinity`.

Three cases cannot be the same in JS, because a JS number cannot carry them:

- An integer beyond 2^53 is rounded (`"9007199254740993"` is
  `9007199254740992`).
- `2.0` is the integer `2` (so `2.0` for a dict is refused as `"an integer"`,
  where C says `"a real"`).
- The last 512 integers of int64 round up to 2^63, which is out of int64. An
  integer refuses them as text and as a number (7.26.26): the int64 maximum
  `"9223372036854775807"` is `"a number out of its range"` in JS, because its
  JS number is 2^63, and C refuses the text of 2^63 ("too big integer").

The test table has these cases, with the result of each kernel.

It returns `{value, refused}`. If `value` is `undefined`, `refused` gives the
cause, and it never contains the value. If `refused` is `null` too, there is
no value: a `null`, `undefined`, or an empty text for a number.

```javascript
command_parameter_from_json(data_type_t.DTP_INTEGER, "42");   // {value: 42, refused: null}
command_parameter_from_json(data_type_t.DTP_INTEGER, 2.7);    // {value: undefined, refused: "a real with decimals"}
command_parameter_from_json(data_type_t.DTP_BOOLEAN, "");     // {value: undefined, refused: null}
command_parameter_from_json(data_type_t.DTP_JSON, "1e309");   // {value: "1e309", refused: null}
command_parameter_from_json(data_type_t.DTP_DICT, '{"a": "x\\u0000"}');
                                    // {value: undefined, refused: "a text that does not read as one"}
command_parameter_from_json(data_type_t.DTP_DICT, {ratio: NaN});
                                    // {value: undefined, refused: "a value with no json text"}
command_parameter_from_json(data_type_t.DTP_INTEGER, 9223372036854775807);
                                    // {value: undefined, refused: "a number out of its range"}
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

(js_json_text_of)=
### `json_text_of(value)`

Gives the JSON text of `value`, or `undefined` when the value has no JSON text
that says the same value (7.26.27). The rules are the rules of
[`command_parameter_from_json()`](#js_command_parameter_from_json) for an object
or a list. A string attribute and
[`kw_set_local_storage_value()`](helpers_kw.md#js_kw_set_local_storage_value)
use it, so the two refuse the same values.

- A string, a finite number, a boolean, `null`, and a plain object or array of
  them have a text.
- These have no text: `undefined`, a function, a symbol, a `BigInt`, `NaN`,
  `Infinity`, a cycle, a `Map`, a `Set`, a `Date`, an instance of a class, an
  object with a `toJSON()`, a function, `undefined` or a hole inside an array,
  a value nested deeper than 2048 levels, and a getter that throws.
- A key whose value is `undefined` or a function is an absent key.

```javascript
json_text_of({a: 1, b: undefined});   // '{"a":1}'
json_text_of("text");                 // '"text"'
json_text_of({w: NaN});               // undefined: its text would say {"w":null}
json_text_of([1, , 3]);               // undefined: a hole
json_text_of(new Date(0));            // undefined: its text would be a string
```

(js_gobj_write_bool_attr)=
### [`gobj_write_bool_attr(gobj, name, value)`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L4356)

Writes a boolean attribute. The value is read as a boolean by the grammar of
[`command_parameter_from_json()`](#js_command_parameter_from_json) (7.26.22+):
`"yes"` and `1` are `true`.

A value that is not a boolean is refused: `"maybe"`, an object, and also NO
value (`undefined`, `null`, `""`). The function logs one `log_error` and
returns `-1`, and **the attribute keeps the value it had**. It does not become
`false`. This is the contract of `json2item()` in the C kernel. It is a
BREAKING change of 7.26.22: up to 7.26.21 an absent value was written `false`
and the function returned `0`, and `"yes"` was written `false` too.

```javascript
gobj_write_bool_attr(gobj, "selectable", true);

// kw has no "selectable": -1, one ERROR, and "selectable" is still true
gobj_write_bool_attr(gobj, "selectable", kw.selectable);

// When the value can be absent, compute it before the write
gobj_write_bool_attr(gobj, "selectable", kw.selectable === true);   // false
```

(js_gobj_write_integer_attr)=
### [`gobj_write_integer_attr(gobj, name, value)`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L4388)

Writes an integer attribute. The value is read as an integer, as
[`gobj_write_bool_attr()`](#js_gobj_write_bool_attr) reads a boolean (7.26.22+):
`"42"` is `42`, and `2.5`, `NaN`, `"12abc"` and `undefined` are refused. Up to
7.26.21 `parseInt()` wrote `2` for `2.5` and `12` for `"12abc"`. A time for
`set_timeout()` is rounded to whole milliseconds before it is written.

(js_gobj_write_str_attr)=
### [`gobj_write_str_attr(gobj, name, value)`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L4420)

Writes a string attribute. `null` and `undefined` are written as `""`, and
any other value that is not a string as its JSON text (7.26.22+). Up to
7.26.21 they were written as the texts `"null"` and `"undefined"`.

A value that has no JSON text (a circular object, such as a gobj or a DOM
node, or a `BigInt`) is refused: one `log_error`, `-1`, and the attribute keeps
its value (7.26.24). In 7.26.22 and 7.26.23 the function threw the exception
of `JSON.stringify()`. A value whose JSON text gives another value is refused
too (7.26.26): `NaN` and `Infinity` were the text `"null"`. The full list is in
[`command_parameter_from_json()`](#js_command_parameter_from_json).
[`gobj_write_attr()`](#js_gobj_write_attr) and
[`gobj_write_attrs()`](#js_gobj_write_attrs) do the same for a string
attribute.

```javascript
gobj_write_integer_attr(gobj, "timeout", "1000");       // 1000
gobj_write_str_attr(gobj, "username", kw.username);     // "" when kw has none
```

(js_gobj_reset_volatil_attrs)=
### [`gobj_reset_volatil_attrs(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.3/src/gobj.js#L4319)

Writes the default value in every attribute that carries `SDF_VOLATIL`.

---

An attribute with `SDF_PERSIST` goes to the store through the callbacks that
[`gobj_start_up()`](bootstrap.md#js_gobj_start_up) receives. See
[Persistence](persistence.md).
