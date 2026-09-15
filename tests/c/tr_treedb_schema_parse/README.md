# tr_treedb_schema_parse test

Tests `parse_schema()` on a column that declares no `flag`.

Every user column is validated against the `cols` topic of
`treedb_system_schema`, where `flag` is an `enum` that is **not** `required`.
Before the fix, a column without `flag` crashed `check_desc_field()`: its value
was NULL, the `required` test did not apply, and the `enum` branch switched on
`json_typeof(NULL)`. It was reachable from `C_TREEDB`'s `create-topic` and from
`open-treedb` with a schema.

The test checks:

1. A column without `flag` parses clean (`parse_schema()` returns 0).
2. A column with an unknown flag is still refused (*"Wrong enum type"*).
3. A treedb with the flag-less column opens and takes a record.

A flag-less column is written like this, in a C schema literal:

```c
'text': {
    'header': 'Text',
    'fillspace': 20,
    'type': 'string'
}
```
