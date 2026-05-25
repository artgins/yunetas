# tr_treedb_delete_instance

Regression coverage for `treedb_delete_instance()`.

The function operates on a SECONDARY `pkey2` index — it removes one
slot of an "instance" from memory **without touching** the on-disk
record, the primary `id` index, or the other secondary indexes.

The whole-record wipe (every index + `tranger2_delete_key()` on the
underlying record) is the job of `treedb_delete_node()`.

The two paths conflate the word "instance" by accident: a tranger2
*instance* is one row in a key's `.md2` file (multiple per key); a
treedb *instance* is one slot in a `pkey2` secondary index (one per
declared `pkey2`).  This test pins the treedb side; the tranger2 side
is not implemented yet (see repo `TODO.md`).

## Scenarios

1. `delete_instance drops secondary, keeps primary`
   Seed three items (`item-1..3`, each with a distinct `version`).
   Call `treedb_delete_instance(item-2, "version")` and verify:
   - return code is `0`,
   - the (`item-2`, `v2`) slot is gone from the `version` pkey2
     index,
   - `item-2` is still reachable via the primary `id` index,
   - the sibling items are untouched in both indexes.

2. `delete_node clears primary index`
   After the prior delete_instance, call
   `treedb_delete_node(item-2, force=true)` and verify the primary
   `id` index entry is gone, while siblings remain.

## Run

```bash
cd build && cmake .. && make && ctest --output-on-failure
```
