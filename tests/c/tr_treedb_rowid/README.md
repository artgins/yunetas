# tr_treedb_rowid test

Tests the id a topic hands out when its `id` column carries the `rowid` flag
and the create sends no `id`.

That id used to be `tranger2_topic_size() + 1`, and `tranger2_topic_size()`
sums the **records** of every key. So deleting a node lowered the total and
the next id landed on one that already existed; every update raised it and the
ids skipped. Two real consequences:

- `__snaps__` is a `rowid` topic. Shoot two snaps, delete the row of the
  first (the documented way to release the assets a snap holds), shoot again:
  the new snap asked for the id of the second, the create was refused as
  *"Node already exists"*, and `treedb_shoot_snap()` logged it as critical,
  which exits a yuno whose tranger runs with `exit_on_error`.
- A topic with `pkey2s` (the agent's `yunos`) does not refuse an existing id
  with a DIFFERENT secondary key: it adds an instance. A new node whose id
  collided became an instance of an unrelated one.

The id is now one past every id the topic ever handed out, kept in the topic's
`topic_var.json` as `last_rowid_id`, and never reused: a snap's id rides the
records it tagged as `user_flag`, so a new snap with a deleted snap's id would
inherit them.

The test checks, on a topic declared like this:

```c
'id': {
    'header': 'Id',
    'fillspace': 10,
    'type': 'string',
    'flag': ['persistent', 'required', 'rowid']
}
```

1. Ids are handed out in sequence, and an update does not move the sequence.
2. After a node is deleted, the next create gets a fresh id and does not
   become an instance of another node.
3. The id of the deleted last node is not handed out again.
4. The counter survives a close and reopen, even when the node that held the
   highest id was deleted before the close.
5. The counter survives a `topic_version` change across a RESTART of the
   tranger. The change re-creates `topic_var.json` from the schema, and the
   counter lived there; in the same process the topic stays open in the
   tranger and keeps it in memory, so the loss shows only with a new tranger,
   which is what a yuno restart is. Re-seeded from the ids alive, the next
   create handed out the deleted highest id again (`4`, the id `d` had).
6. The same for `__snaps__`: delete a snap row, shoot again, reload, shoot
   again.
