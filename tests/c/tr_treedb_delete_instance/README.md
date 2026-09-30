# tr_treedb_delete_instance

Regression coverage for `treedb_delete_instance()`, and for what a delete, a
save, a link and a snapshot must do when a key has several instances.

`treedb_delete_instance()` deletes ONE instance, one `pkey2` index slot: every
`.md2` row of (id, pkey2 value) is tombstoned on disk, the primary `id` index
and the other secondary indexes stay untouched, and the instance leaves the
hooks of its parents in memory (the children it holds go to the primary).

The whole-key wipe (every index + `tranger2_delete_key()` on the underlying
record) is the job of `treedb_delete_node()`.

## Cases

The binary runs 29 cases, in this order. The first four share one database
(`tr_delete_instance`, schema `schema_sample.c`: topic `items`, pkey2
`version`); every other case opens its own database and closes it again.
"7.25.4" is the release each regression was seen in.

### Shared database (`do_test()`)

1. **`delete_instance drops secondary, keeps primary`**
   (`test_delete_instance_drops_secondary_keeps_primary`). Seeds `item-1..3`,
   each with its own `version`, and deletes the instance (`item-2`, `v2`):
   the call answers `0`, the slot is gone from the `version` index, `item-2`
   is still reachable by the primary `id` index, and the sibling items are
   untouched in both indexes.
2. **`an instance a snapshot holds is not deleted`**
   (`test_instance_held_by_a_snap`). A snap freezes an instance, which is then
   updated (a save is untagged, so the node in memory carries tag `0` while
   the frozen record is still under it). The delete is refused, and the
   refused delete leaves the instance in memory; an instance no snap holds is
   deleted. `force` does NOT override the snap (it is about links);
   `ignore_snaps` does.
3. **`a snapshot guard that cannot read refuses the delete`**
   (`test_guard_that_cannot_read_refuses`). The md2 of the key is cut behind
   treedb's back, so the guard cannot read the key's records: the delete is
   refused (fails closed) instead of taking a failed read for "no snapshot
   holds it".
4. **`delete_node clears primary index`**
   (`test_delete_node_clears_everything`). After case 1,
   `treedb_delete_node(item-2, force=true)` removes the primary `id` entry
   and the on-disk rows, and the siblings remain.

### Own databases

5. **`delete_instance is durable across reopen (multi-row)`**
   (`test_durable_delete_across_reopen`). An instance with several md2 rows
   (create, update) is deleted: every row is tombstoned, so after a close and
   reopen it does not come back from an older row, and the other instances
   and the primary survive. A pkey2 value with a `/` (`v/4`) is deleted too,
   with no log (it was the id of the delete's transient rt_disk feed, and
   logged "Invalid rt id" / "Cannot open rt").
6. **`a delete_instance that cannot read every row refuses`**
   (`test_delete_that_cannot_read_refuses`). The md2 of one key is cut (a
   short read), and the content of another (`__offset__/__size__` out of
   range): each delete answers `-1` with *"Cannot delete instance, cannot
   read every row of its key"*, and the instance stays in memory and after a
   reopen. 7.25.4 ignored the iterator's `load_failed`, tombstoned what it
   had read and answered `0`; the instance came back at the next open.
7. **`a delete_instance whose tombstone fails part way keeps the instance`**
   (`test_tombstone_that_fails_part_way`). An instance with two rows
   (created, then updated); the write of the second tombstone fails (the test
   wraps `write()`, see `CMakeLists.txt`). The rows go oldest first, so the
   newest row is still alive: the delete answers `-1`, the instance stays in
   memory, and a reopen loads it from the newest row. (7.25.4 answered `0`,
   and the instance came back older.)

The cases from here on use the schema `schema_links.c`: `parents` and `kids`,
both with a pkey2, linked by a LIST hook (`kids`), a DICT hook over a list
fkey (`tags`) and a DICT hook over a string fkey (`pets`). Each one reopens
its database at the end and checks what the reload says.

8. **`a delete of a parent sees the children of every instance`**
   (`test_delete_node_sees_children_of_every_instance`). Kids `a` and `b` hang
   from the NON-primary instance P/v2. A delete of P without force is refused;
   with force they are unlinked and saved, and after the reopen neither names
   P. 7.25.4 looked at P/v1 alone: P was deleted, and the reopen said "Node
   not found".
9. **`a delete of a child takes every instance out of its parents' hooks`**
   (`test_delete_node_unhooks_every_instance`). x/v2 and y/v2 hang from O2
   (list and dict hook); the primaries hang from nothing. A delete of x or y
   without force is refused; with force O2's hooks are left empty, and after
   a forced delete of O2 and the reopen, x, y and O2 are gone. 7.25.4 left the
   ghosts, and the forced delete of O2 saved them back.
10. **`a deleted child instance leaves the hooks of its parents`**
    (`test_deleted_instance_leaves_parents_hooks`). After `delete_instance` of
    x/v2, y/v2 (hanging from O) and z/v2 (from Q), O holds nothing, a delete
    of O without force goes, and a forced delete of Q does not save z/v2 back;
    after the reopen no v2 instance is there and each primary is.
11. **`the primary takes the place of a deleted instance`**
    (`test_primary_takes_place_of_deleted_instance`). O's list hook holds
    x/v2; the primary x/v1 names O too. After the delete of x/v2, O holds
    x/v1, as the reopen says.
12. **`a deleted parent instance hands its children to the primary`**
    (`test_deleted_parent_instance_hands_children_to_primary`). The children
    of P/v2 go to P/v1 in memory (7.25.4 left them under no visible parent
    until a reload), a delete of P without force is refused for them, and the
    reopen hangs them from P/v1.
13. **`a refused forced delete puts back every instance`**
    (`test_refused_forced_delete_puts_every_instance_back`). The key cannot be
    deleted (its directory made read-only). For the parent P, kid `a` of P/v2,
    unlinked and saved by the forced delete, goes back into P/v2 and names P
    again, on disk too. For the child x, x/v2, taken out of O2's hooks (list
    and dict), goes back in its place.
14. **`a save of a node no index holds is refused`**
    (`test_save_of_unindexed_node_refused`). A deleted instance (x/v2) and a
    node of a deleted key (w), kept by a pointer, are not saved; after the
    reopen neither is back and x/v1 is. 7.25.4 wrote them back.
15. **`after a reopen the instance of the primary is the primary`**
    (`test_instance_of_primary_is_primary_after_reopen`). The pkey2 lookup of
    the primary's value answers the primary node after a create, a save, a
    reopen and a `delete_instance`; a new instance does not become the primary
    in memory (the reload makes the newest record the primary); updates
    through the instance and through the primary each write one row and both
    survive the reopen. 7.25.4 answered a second object after a reopen: the
    update through it was lost, and its pointer was freed by the next save of
    the primary.
16. **`a save of a node whose pkey2 value changed in place is refused`**
    (`test_save_of_moved_pkey2_refused`). x/v1 changed in place to v9, and x/v2
    to v1 (the primary's value), are not saved (an ERROR naming both values);
    put back, both save; after the reopen x has v1 and v2 and no v9. 7.25.4
    wrote x/v9 to disk and re-pointed the slot of v1 to x/v2.
17. **`a create indexes a defaulted pkey2 by its record's value`**
    (`test_create_indexes_default_pkey2`). The topic `defaulted` has a pkey2
    with a default (v0); a create without it is the instance d/v0 in memory,
    is updated and saved, and is d/v0 after the reopen. 7.25.4 filled the slot
    of the kw's value, "".
18. **`an unlink clears the ref in every instance of the child`**
    (`test_unlink_clears_every_instance_of_the_child`). a/v1 hangs from P
    through both hooks, and a/v2 inherits both refs at its create. The unlinks
    of a/v1 leave a/v2 naming nothing either (saved), P holds no child, and
    after the reopen a/v1 is the primary and neither instance names P. 7.25.4
    left a/v2 naming P: a delete of P without force went, and a new P adopted
    `a` at the reopen.
19. **`a delete of a parent sees the unheld instances that name it`**
    (`test_delete_sees_unheld_instances_naming_it`). a/v1 hangs from P, a/v2
    inherits the ref, and a/v1 is moved to Q: P holds nothing and a/v2 names
    it. A delete of P without force is refused; forced, a/v2 stops naming P
    (saved) and a/v1 stays in Q; after the reopen P is gone and the primary of
    `a` names nothing that is gone. 7.25.4 deleted P: "Node not found" at the
    reopen.
20. **`a forced delete leaves no ref in any instance of a child`**
    (`test_forced_delete_leaves_no_ref_in_any_instance`). a/v1 hangs from P
    through `tags`, a/v2 through `kids`. The forced delete of P saves no
    record that still names P. 7.25.4 saved a/v2 with its `tags` still naming
    P: the delete's own record carried the dangling ref.
21. **`a relink of an instance its parent does not hold is quiet`**
    (`test_relink_of_a_sibling_instance_is_quiet`). b/v1 hangs from P through
    `kids`, b/v2 inherits the ref, and P's hook holds b/v1. Moving b/v2 to Q
    logs nothing and P keeps b/v1. 7.25.4 logged "Child data not found in
    dict parent hook" -- of a list hook.
22. **`with a snap active a key created after it has no primary`**
    (`test_snap_key_with_instances_and_no_primary`). With a snap active, the
    id index loads only the records the snap tagged, so a key created after
    it has instances and no primary. A create of such an instance takes the
    slot (7.25.4 made a SECOND object, and a later C_NODE delete-node
    tombstoned every row of P through it: P was gone after the reopen, its kid
    naming it); and a delete of such an instance does not log
    "delete_primary_node() FAILED" on a delete that went.
23. **`with two pkey2s a save keeps the primary in its slot`**
    (`test_save_keeps_the_primary_in_its_slot`, its own schema `multi`,
    pkey2s `a` and `b`). P (a=1, b=1) is the primary of x, Q (a=1, b=2) an
    instance; they share the slot a=1, which holds P. An update (save) of Q
    leaves the slot a=1 on P and keeps Q in its own slot b=2, so what C_NODE's
    delete-node {x, a: 1} does (delete the instance of a=1 only when it is not
    the primary) deletes nothing, and x is there after the reopen. 7.25.4
    moved the slot to Q, the delete tombstoned every row with a=1 -- the
    primary's too -- and x was gone after the reopen.
24. **`the gc holds the asset an instance names`**
    (`test_gc_holds_the_asset_of_an_instance`, its own schema `docs` with a
    file fkey). d/v1 holds photo A, d/v2 photo B. After a reopen only the
    primary (d/v2) is linked, and the gc keeps A -- row and bytes -- while
    d/v1 names it. 7.25.4 collected A.
25. **`the unrefs of a write that does not go are taken back`**
    (`test_unrefs_are_taken_back`). (1) The unlink of a/v1 cannot save (the
    key of `a` read-only): a/v2, which the unlink had made stop naming P,
    names it again, and a/v1 is back in P's hooks. (2) The forced delete of P
    cannot delete its key (P read-only) after saving a/v2 naming P no more:
    a/v2 names P again, in memory and on disk.
26. **`a forced delete keeps the newest record of each key it saves`**
    (`test_forced_delete_keeps_the_newest_record`). (1) a/v1 moved to Q wrote
    the newest record of `a`: after the forced delete of P (which saves a/v2)
    and the reopen, a/v1 is still the primary, in Q. (2) a/v2 created after
    a/v1 is the newest record: after the forced delete (which saves both) and
    the reopen, a/v2 is still the primary. 7.25.4 left the instance it saved
    LAST as the newest record, and the reload picked it.
27. **`a delete taken back keeps the newest record of each key`**
    (`test_taken_back_delete_keeps_the_newest_record`). The same as the first
    half of case 26 with P's key read-only: the refused delete saves a/v2
    back, and a/v1 writes the newest record again, so after the reopen a/v1
    is the primary and Q holds it.
28. **`a relink through a dict hook keeps the sibling instance`**
    (`test_relink_through_a_dict_hook_keeps_the_sibling`). b/v2 moved from P
    to Q through `pets` (a dict hook over a string fkey) leaves b/v1 in P's
    dict hook, naming P, with no log. 7.25.4 dropped the slot by id -- b/v1
    with it -- and logged "Child data not found in dict parent hook".
29. **`a shot keeps the newest record of each key`**
    (`test_shot_keeps_the_newest_record`). a/v1 is tagged by snap s1, a/v2 is
    created after it. The shot of s2 clones the record of a/v1 and then
    writes the record of a/v2 again, so a/v2 is the primary after the reopen;
    with s2 activated, a/v1 is. 7.25.4 left the clone the newest record, and
    a/v1 came back as the primary.

## Run

```bash
ctest -R test_tr_treedb_delete_instance --output-on-failure --test-dir build
```
