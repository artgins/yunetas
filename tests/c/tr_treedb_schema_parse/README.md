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
4. A column flagged both `hook` and `fkey` is refused by `parse_schema()`
   (*"A column cannot be both 'hook' and 'fkey'"*).
5. `treedb_create_topic()` refuses a topic with such a column, instead of only
   logging it (*"Topic refused: a column is both 'hook' and 'fkey'"*).
6. A schema with one hook on an fkey parses clean twice (`parse_hooks()`
   writes its `fkey` mark into the schema, and the second pass meets it); two
   hooks on one fkey are refused (*"Only can be one fkey"*).
7. `treedb_create_topic()` refuses a topic with no `id` column, or with no
   columns, before anything is on disk (*"Topic refused: bad columns"*).
8. `treedb_create_topic()` refuses bad pkey2s before the topic exists
   (*"Topic refused: bad pkey2s"*): a pkey2 named twice, the pkey `id`, a name
   with a backtick, the tkey. Their index was built wrong: a node stored
   inside itself, two lists on one id. Each one is an ERROR for a C caller,
   and a WARNING inside the window of a peer's data
   (`treedb_set_peer_data(TRUE)`, the last case).
9. `treedb_create_topic()` refuses a topic whose name or hook id holds a
   `^` -- the separator of the parts of a reference, `topic^id^hook`, so every
   ref to its nodes was undecodable --, and a pkey2 that names no column of
   the topic (its index held nothing): *"Topic refused: a name of its
   references or of its keys"*, an ERROR for a C caller, a WARNING in the
   window of a peer's data. And `treedb_create_node()` refuses an `id` that is
   not a string (*"Field 'id' must be a string"*; an integer was read with an
   ERROR of `kw_get_str()`, and the node made with no id or a uuid).
10. `treedb_create_topic()` refuses a topic whose list id, the id of the list
   that loads an index (`treedb-<gclass>-<gobj>-<treedb>-<topic>-<key>`), does
   not fit `NAME_MAX` (a topic name of 250 bytes): an ERROR always (*"Topic
   refused: the id of a list of its indexes is longer than NAME_MAX"*), and no
   directory on disk. Cut, two lists of the topic shared one id.

Items 8 and 9 are the rules of a NEW topic. A topic already ON DISK (its
`topic_desc.json` is there), opened by a C caller, keeps its pkey2s and its
list ids as they are, each fault an ERROR (*"Topic on disk opened as it was,
..."*): refusing them would take the topic from a store that has lived with
them.
That case is tested in
[`c_treedb_system_schema`](../c_treedb_system_schema/README.md) (13d6).

The pkey2s a topic may take, in a C schema literal:

```c
'topic_name': 'notes',
'pkey': 'id',
'tkey': 't',
'pkey2s': ['serial', 'email'],      // not 'id', not the tkey 't', each once
```

A node that is both a child and a parent carries two columns, the hook and the
fkey, never one with both flags:

```c
'users': {
    'header': 'Users',
    'fillspace': 20,
    'type': 'array',
    'flag': ['hook'],
    'hook': {
        'users': 'departments'
    }
},
'manager': {
    'header': 'Manager',
    'fillspace': 20,
    'type': 'array',
    'flag': ['fkey']
}
```

A flag-less column is written like this, in a C schema literal:

```c
'text': {
    'header': 'Text',
    'fillspace': 20,
    'type': 'string'
}
```
