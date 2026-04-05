# c_node_link_events test

Tests TreeDB link/unlink events at the `C_NODE` GClass level: whenever a node is linked or unlinked via a hook, the framework must publish `EV_TREEDB_NODE_LINKED` / `EV_TREEDB_NODE_UNLINKED` with the right payload and subscriber set.

See also `tests/c/tr_treedb_link_events` for the lower-level (TreeDB-API) counterpart, and `CLAUDE.md` for the hook/fkey rules.

## Run

```bash
ctest -R test_c_node_link_events --output-on-failure --test-dir build
```
