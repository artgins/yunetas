# tr_treedb_delete_instance

Regression coverage for `treedb_delete_instance()`.

The function deletes ONE instance, one `pkey2` index slot: every `.md2` row
of (id, pkey2 value) is tombstoned on disk, the primary `id` index and the
other secondary indexes stay untouched, and the instance leaves the hooks of
its parents in memory (the children it holds go to the primary).

The whole-key wipe (every index + `tranger2_delete_key()` on the underlying
record) is the job of `treedb_delete_node()`.

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

3. `a snapshot guard that cannot read refuses the delete`
   The md2 of the key is cut, so the snapshot guard cannot read the key.
   The delete is refused (it fails closed).

4. `a delete_instance whose tombstone fails part way keeps the instance`
   An instance with two rows (created, then updated). The write of the
   second tombstone fails (the test wraps `write()`, see `CMakeLists.txt`).
   The rows go oldest first, so the newest row is still alive: the delete
   answers `-1`, the instance stays in memory, and a reopen loads it from
   the newest row. (7.25.4 answered `0`, and the instance came back older.)

5. `delete_instance` / `delete_node` with LINKS (schema `schema_links.c`:
   `parents` and `kids`, both with a pkey2, linked by a LIST hook, a DICT
   hook over a list fkey and a DICT hook over a string fkey). Each one has its
   own database and reopens it at the end:
   - `a delete of a parent sees the children of every instance`: a child of
     the NON-primary instance P/v2 refuses a delete of P without force; with
     force it is unlinked and saved. 7.25.4 deleted P and left the child
     naming it ("Node not found" at the reopen).
   - `a delete of a child takes every instance out of its parents' hooks`: x/v2
     hangs from O2; a delete of x (primary x/v1) is refused without force, and
     with force O2's hooks are left empty. 7.25.4 left the ghost, and a forced
     delete of O2 saved it back: x was there after the reopen.
   - `a deleted child instance leaves the hooks of its parents`: after
     `delete_instance` of x/v2, O holds nothing, a delete of O without force
     goes, and a forced delete of another parent does not save the instance
     back.
   - `the primary takes the place of a deleted instance`: O's list hook holds
     x/v2, the primary x/v1 names O too; after the delete O holds x/v1, as the
     reopen says.
   - `a deleted parent instance hands its children to the primary`: the
     children of P/v2 go to P/v1, as the reopen says, and a delete of P
     without force is refused for them.
   - `a save of a node no index holds is refused`: a deleted instance and a
     deleted key, kept by a pointer, are not saved (7.25.4 wrote them back).
   - `after a reopen the instance of the primary is the primary`: the
     pkey2 lookup of the primary's value answers the primary node after a
     create, a save, a reopen and a delete_instance; a new instance does not
     become the primary in memory (the reload makes the newest record the
     primary); updates through the instance and through the primary each
     write one row and both survive the reopen. 7.25.4 answered a second
     object after a reopen: the update through it was lost, and the pointer
     was freed by the next save of the primary.
   - `a save of a node whose pkey2 value changed in place is refused`: x/v1
     changed in place to v9, and x/v2 to v1 (the primary's value), are not
     saved (ERROR naming both values); put back, both save; after the reopen
     x has v1 and v2 and no v9. 7.25.4 wrote x/v9 to disk and re-pointed the
     slot of v1 to x/v2.
   - `a create indexes a defaulted pkey2 by its record's value`: the topic
     `defaulted` has a pkey2 with a default (v0); a create without it is the
     instance d/v0 in memory, is updated and saved, and is d/v0 after the
     reopen. 7.25.4 filled the slot of the kw's value, "".
   - `a forced delete keeps the newest record of each key it saves`: the
     instances a forced delete of P saves (one no hook holds, the child it
     unlinks) leave the newest record of their key on the instance that wrote
     it before: a/v1 moved to Q stays the primary, in Q, and a new instance
     a/v2 stays the primary of the next reload. 7.25.4 saved the child it
     unlinked last, and after the reopen a/v1 was the primary: the new
     instance a/v2 lost.
   - `a delete taken back keeps the newest record of each key`: the same with
     P's key read-only; the refused delete saves a/v2 back, and a/v1 writes
     the newest record again.
   - `a relink through a dict hook keeps the sibling instance`: b/v2 moved
     from P to Q through `pets` (a dict hook over a string fkey) leaves b/v1 in
     P's dict hook, naming P. 7.25.4 dropped the slot by id.
   - `a shot keeps the newest record of each key`: a shot that clones the
     record of a/v1 (tagged by an earlier snap) writes the record of the new
     instance a/v2 again after the clone: a/v2 is the primary after the
     reopen, a/v1 with the snap active. 7.25.4 left the clone the newest.

## Run

```bash
cd build && cmake .. && make && ctest --output-on-failure
```
