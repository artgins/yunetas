# tr_treedb_failed_save test

A treedb write whose **save fails** is taken back in memory.

`treedb_update_node()`, `treedb_link_nodes()`, `treedb_unlink_nodes()`,
`treedb_replace_links()` and `treedb_autolink()` change the node in memory
first (its fields, its fkeys and the hooks of its parents), then save it.
When the save fails (here the files of the key are made read-only), the node
in memory goes back to what the disk has, and no event of the write is told.
When the files are writable again, the same writes work, and a reload from
disk says what memory said.

## What it pins

1. A failed update, link, unlink, replace of a single fkey,
   `treedb_replace_links()` and `treedb_autolink()`: the call answers `NULL`
   or `-1`, memory is as the disk, and no event is told.
2. `treedb_update_node_and_links()` (the `update-node` with `autolink` of
   `C_NODE`): fields, links and save are ONE write, taken back whole.
3. `treedb_clean_node()`, and a stale ref it removes (its hook is gone, or
   fills another column): the ref goes back into its field alone, never
   linked.
4. A forced `treedb_delete_node()` that is refused (a child that cannot be
   saved unlinked, a key that cannot be deleted): the node keeps its links,
   and the children unlinked before the failure are put back, on disk too.
   When the put-back of a child fails too, that child stays unlinked, in
   memory as on disk, an ERROR says so, and its unlink events are told.
5. A take-back puts every child back in its place in the hooks of its
   parents: the order of a hook is the one it had before the write.
6. `treedb_autolink()` refuses a ref whose hook fills another column than
   the one it arrives in, as `treedb_replace_links()` does.
7. A forced delete tells its unlinks after the node left the indexes, and a
   save of the node from one of those callbacks is refused (*"Cannot save a
   node that is being deleted"*). A read-only file cannot make that second
   save fail (the files of the key are already open), so the test fails the
   writes of that key in its own `__wrap_write()` (see `CMakeLists.txt`).
8. A take-back puts a child back into the INSTANCE of the parent that held
   it. A parent topic with a pkey2 has several instances of one id, and a ref
   names the id alone: an unlink from the second instance (list hook and
   dict hook), and a move of a single fkey away from it, whose save fails,
   leave the child in that instance, in its place, and nothing in the primary.
9. Two snaps active on disk: the open deactivates all but the last; a
   deactivation that cannot be saved leaves the snap active in memory as on
   disk, and a replica does not try.
10. A take-back puts a child back into EVERY instance of the parent that
    held it. x hangs from P/v1 and P/v2 (list hook and dict hook): an unlink
    from P/v1 whose save fails, and a forced delete of P/v1 refused by a
    failed save, leave x in both instances, in its place.
11. A forced delete of a node with children and a parent tells its events in
    their order: the unlink and the save of each child, then the unlink of
    the node from its parent, then the delete. The delete tells the events of
    its children itself (they are not held one by one).

## Run

```bash
ctest -R '^test_tr_treedb_failed_save$' --output-on-failure --test-dir build
```
