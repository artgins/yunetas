# c_node_failed_save test

Tests an `update-node` with `autolink` of `C_NODE` whose **save fails**.

`C_NODE` writes such an update as ONE write,
`treedb_update_node_and_links()`: the fields, the links and the save. When the
save fails, the write is taken back whole, and none of its events is
published.

## What it pins

1. **update + autolink** (the method `gobj_update_node()`): the answer is
   `NULL`, alice keeps her old username and has no department in memory,
   `direction` does not hook her, and no `EV_TREEDB_NODE_LINKED` /
   `EV_TREEDB_NODE_UPDATED` is published;
2. the same through the **`update-node` command**: it answers `-1`, and the
   rest is the same;
3. **create + autolink**: the create goes through, the save of its links
   fails. The answer is `NULL`, the log says *"Node created, but its links
   cannot be saved (autolink): the node stays without them"*, the node is in
   memory without its links, and only `EV_TREEDB_NODE_CREATED` is published;
4. a reload from disk (a new tranger) says what memory said;
5. the same update, retried with the disk writable, works: one
   `EV_TREEDB_NODE_LINKED`, and the update published.

A read-only file cannot make these saves fail: once a key is written its files
stay open, and case 3 needs the create to go through and the save after it to
fail. So the test fails the writes into the files of one key in its own
`__wrap_write()` (`-Wl,--wrap=write` in `CMakeLists.txt`, this target only).

## Run

```bash
ctest -R test_c_node_failed_save --output-on-failure --test-dir build
```
