# c_node_initial_load test

Tests the `initial_load` seed of the `C_NODE` GClass: the records a treedb
declares it cannot come up without.

What it checks:

- The seed comes up complete on the **first** start, whatever the order of
  its topics (the child is declared before its parent here). Records first,
  links second, the way `treedb_open_db()` loads a store from disk.
- Every seed record is immutable.
- The links a seed is declared with cannot be cut: not by `unlink-nodes`, not
  by an autolink `update-node` that omits them, not by deleting the parent
  with `force`, and not by a `link-nodes` into a **single-valued** fkey,
  which replaces instead of adding.
- The links a person adds to a seed afterwards CAN be cut, and a record no
  seed hangs from can be deleted.
- A second start creates nothing and writes no link.
- `print-tranger` walks the `path` it is handed itself: a segment longer than
  255 bytes and a segment under a string (`` directory`x ``) are refused with
  `-1` and a WARNING *"print-tranger path refused"* (they were ERRORs of
  `kw_find_path()`), and a string at the end of the path (`directory`) is
  answered as it is (it was an ERROR of `kw_collapse()`). The test installs
  an authz checker that grants every permission: `print-tranger` asks `read`.

What a seed gets wrong is a WARNING when the seeds are a peer's (an
`open-treedb` from outside); that case is tested in
[`c_treedb_system_schema`](../c_treedb_system_schema/README.md) (Test 13d3).

The schema carries the two fkey shapes on purpose: `users.departments` is a
list, which a link ADDS to, and `machines.department` is a string, which a
link REPLACES. Only the second one can lose a seed link to `link-nodes`, so a
test with list fkeys alone cannot see that guard fail.

See `yunos/c/yuno_agent/YUNO_TREEDB.md` §3.10 for the contract.

## Run

```bash
ctest -R test_c_node_initial_load --output-on-failure --test-dir build
```
