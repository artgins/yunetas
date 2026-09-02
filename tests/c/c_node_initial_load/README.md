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

The schema carries the two fkey shapes on purpose: `users.departments` is a
list, which a link ADDS to, and `machines.department` is a string, which a
link REPLACES. Only the second one can lose a seed link to `link-nodes`, so a
test with list fkeys alone cannot see that guard fail.

See `yunos/c/yuno_agent/YUNO_TREEDB.md` §3.10 for the contract.

## Run

```bash
ctest -R test_c_node_initial_load --output-on-failure --test-dir build
```
