# TreeDB Helpers

Utilities for interacting with the Yuneta **TreeDB** — the
schema-driven graph database built on top of `timeranger2`. See the
[Timeranger2 API](../timeranger2/timeranger2.md) and the TreeDB
section of `CLAUDE.md` for the underlying semantics.

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

| Function | Purpose |
|---|---|
| `treedb_hook_data_size(value)` | Count items referenced by a hook field, with caching. |
| `treedb_decoder_fkey(col, fkey)` | Parse a foreign-key reference string into its `{topic, id, hook}` components. |
| `treedb_encoder_fkey(col, fkey)` | Build a canonical fkey reference string `"topic^id^hook"`. |
| `treedb_decoder_hook(col, hook)` | Parse a hook reference. |
| `treedb_get_field_desc(col)` | Build a field descriptor from a column definition. |
| `template_get_field_desc(key, value)` | Build a field descriptor from a template entry. |
| `create_template_record(template, kw)` | Instantiate a new record from a template definition. |

## Fkey format

TreeDB references children from parents via **hooks** (in-memory only)
and children point back to parents via **fkeys** (persisted). An fkey
value is a string with three `^`-separated fields:

```
topic^parent_id^hook_name
```

For example, a `user` record under the `administration` department
might have:

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
`g_rowid`. See `CLAUDE.md` for the full rules and traced examples.
:::
