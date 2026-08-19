# TreeDB Helpers

Utilities for interacting with the Yuneta **TreeDB** — the
schema-driven graph database built on top of `timeranger2`. See the
[Timeranger2 API](../timeranger2/timeranger2.md) and the
[TreeDB crash course](../../../../yunos/c/yuno_agent/YUNO_TREEDB.md) for the
underlying semantics.

## Available helpers

```javascript
import {
    treedb_hook_data_size,
    treedb_decoder_fkey,
    treedb_encoder_fkey,
    treedb_decoder_hook,
    treedb_get_field_desc,
    template_get_field_desc,
    create_template_record,
} from "@yuneta/gobj-js";
```

(js_treedb_hook_data_size)=
### [`treedb_hook_data_size(value)`](https://github.com/artgins/gobj-js/blob/7.13.2/src/lib_treedb.js#L145)

Counts the items that a hook field references.

(js_treedb_decoder_fkey)=
### [`treedb_decoder_fkey(col, fkey)`](https://github.com/artgins/gobj-js/blob/7.13.2/src/lib_treedb.js#L177)

Reads a foreign-key reference, and gives its topic, its identifier and its hook.

(js_treedb_encoder_fkey)=
### [`treedb_encoder_fkey(col, fkey)`](https://github.com/artgins/gobj-js/blob/7.13.2/src/lib_treedb.js#L241)

Builds the canonical form of a foreign-key reference, `"topic^id^hook"`.

(js_treedb_decoder_hook)=
### [`treedb_decoder_hook(col, hook)`](https://github.com/artgins/gobj-js/blob/7.13.2/src/lib_treedb.js#L268)

Reads a hook reference.

(js_treedb_get_field_desc)=
### [`treedb_get_field_desc(col)`](https://github.com/artgins/gobj-js/blob/7.13.2/src/lib_treedb.js#L398)

Builds the descriptor of a field from the definition of a column.

(js_template_get_field_desc)=
### [`template_get_field_desc(key, value)`](https://github.com/artgins/gobj-js/blob/7.13.2/src/lib_treedb.js#L458)

Builds the descriptor of a field from one entry of a template.

(js_create_template_record)=
### [`create_template_record(template, kw)`](https://github.com/artgins/gobj-js/blob/7.13.2/src/lib_treedb.js#L535)

Builds a new record from the definition of a template.

## Fkey format

TreeDB references children from parents via **hooks** (in-memory only)
and children point back to parents via **fkeys** (persisted). An fkey
value is a string with three `^`-separated fields:

```
topic^parent_id^hook_name
```

For example, a `user` record under the `administration` department
can have:

```
departments^administration^users
```

meaning *"my parent is in topic `departments`, with primary key
`administration`, and I belong to its `users` hook."*

The `treedb_encoder_fkey` / `treedb_decoder_fkey` helpers hide this
encoding so application code rarely has to look at the raw string.

:::{important}
In TreeDB, **link/unlink operations persist only the child node** (the
one carrying the fkey field), never the parent. Every link or unlink
appends a new record to the child's topic in `timeranger2`, bumping its
`g_rowid`. See the [TreeDB crash course](../../../../yunos/c/yuno_agent/YUNO_TREEDB.md) for the full rules and traced examples.
:::
