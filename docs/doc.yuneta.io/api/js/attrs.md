# Attribute Access

Attributes are declared in a schema table using `SDATA()` macros. Each
attribute has a type, name, flags, default value, and description.

## Declaring a schema

```javascript
import { SDATA, SDATA_END, data_type_t, sdata_flag_t } from "@yuneta/gobj-js";

const attrs_table = [
    SDATA(data_type_t.DTP_STRING,  "url",        sdata_flag_t.SDF_RD,      "", "Server URL"),
    SDATA(data_type_t.DTP_INTEGER, "timeout",    sdata_flag_t.SDF_RD,       0, "Timeout ms"),
    SDATA(data_type_t.DTP_BOOLEAN, "connected",  sdata_flag_t.SDF_RD,   false, "Connection state"),
    SDATA(data_type_t.DTP_STRING,  "saved_key",  sdata_flag_t.SDF_PERSIST,  "", "Persisted value"),
    SDATA_END()
];
```

**Data types (`data_type_t`):** `DTP_STRING`, `DTP_BOOLEAN`,
`DTP_INTEGER`, `DTP_REAL`, `DTP_LIST`, `DTP_DICT`, `DTP_JSON`,
`DTP_POINTER`.

**Flags (`sdata_flag_t`):** `SDF_RD`, `SDF_WR`, `SDF_PERSIST`,
`SDF_STATS`, `SDF_AUTHZ_R`, `SDF_AUTHZ_W`, `SDF_REQUIRED`, `SDF_VOLATIL`.

## Generic read / write

```javascript
// `path` accepts back-tick navigation to walk into child gobjs: "child`subattr"
gobj_read_attr (gobj, name, src)
gobj_write_attr(gobj, path, value, src)

gobj_read_attrs (gobj, include_flag, src)        // → JSON object of matching attrs
gobj_write_attrs(gobj, kw, include_flag, src)    // include_flag filters which are written

gobj_has_attr(gobj, name)                        // → boolean
```

## Typed reads (no `src`)

```javascript
gobj_read_bool_attr   (gobj, name)
gobj_read_integer_attr(gobj, name)
gobj_read_str_attr    (gobj, name)
gobj_read_pointer_attr(gobj, name)
```

## Typed writes (no `src`)

```javascript
gobj_write_bool_attr   (gobj, name, value)
gobj_write_integer_attr(gobj, name, value)
gobj_write_str_attr    (gobj, name, value)
```

:::{note}
Attributes marked `SDF_PERSIST` are automatically saved/loaded via the
persistence callbacks registered at bootstrap. See
[Persistence](persistence.md) for details.
:::
