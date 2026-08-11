---
title: 'JS: Attributes'
description: >-
  Declaring the attribute schema of a gclass with SDATA, and reading and
  writing the attributes of a gobj.
---

# Attributes

A gclass declares its attributes in a table. Each attribute has a type, a name,
a set of flags, a default value and a description.

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js)

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
### [`SDATA(type, name, flag, default_value, description)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L157)

Declares one attribute. The table finishes with
[`SDATA_END()`](commands.md#js_SDATA_END).

(js_data_type_t)=
### [`data_type_t`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L85)

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
### [`sdata_flag_t`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L109)

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

---

## Read

:::{note}
A `path` accepts the back-tick to reach an attribute of a child gobj:
``"child`subattr"``.
:::

(js_gobj_has_attr)=
### [`gobj_has_attr(gobj, name)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L3131)

Tells if a gobj has an attribute with that name.

(js_gobj_read_attr)=
### [`gobj_read_attr(gobj, name, src)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L3142)

Reads an attribute of any type. `src` names the gobj that asks, for the
authorization and for the log.

(js_gobj_read_attrs)=
### [`gobj_read_attrs(gobj, include_flag, src)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L3165)

Reads every attribute whose flags match `include_flag`, as one object.

(js_gobj_read_bool_attr)=
### [`gobj_read_bool_attr(gobj, name)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L3201)

Reads a boolean attribute.

(js_gobj_read_integer_attr)=
### [`gobj_read_integer_attr(gobj, name)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L3232)

Reads an integer attribute.

(js_gobj_read_str_attr)=
### [`gobj_read_str_attr(gobj, name)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L3255)

Reads a string attribute.

(js_gobj_read_pointer_attr)=
### [`gobj_read_pointer_attr(gobj, name)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L3278)

Reads an attribute that holds a value which is not JSON. The `subscriber`
attribute of the subscription model uses it.

(js_gobj_hsdata)=
### [`gobj_hsdata(gobj)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L1044)

Gives the whole store of the attributes of a gobj. It is an internal handle, and
the functions above are the way to read an attribute.

---

## Write

(js_gobj_write_attr)=
### [`gobj_write_attr(gobj, path, value, src)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L3297)

Writes an attribute of any type.

(js_gobj_write_attrs)=
### [`gobj_write_attrs(gobj, kw, include_flag, src)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L3438)

Writes every attribute of `kw` whose flags match `include_flag`.

(js_gobj_write_bool_attr)=
### [`gobj_write_bool_attr(gobj, name, value)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L3463)

Writes a boolean attribute.

(js_gobj_write_integer_attr)=
### [`gobj_write_integer_attr(gobj, name, value)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L3491)

Writes an integer attribute.

(js_gobj_write_str_attr)=
### [`gobj_write_str_attr(gobj, name, value)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L3519)

Writes a string attribute.

(js_gobj_reset_volatil_attrs)=
### [`gobj_reset_volatil_attrs(gobj)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/gobj.js#L3426)

Writes the default value in every attribute that carries `SDF_VOLATIL`.

---

An attribute with `SDF_PERSIST` goes to the store through the callbacks that
[`gobj_start_up()`](bootstrap.md#js_gobj_start_up) receives. See
[Persistence](persistence.md).
