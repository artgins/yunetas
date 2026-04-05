# tr_treedb_link_events test

Tests TreeDB **link-event subscriptions** at the TreeDB API level: subscribers must receive `EV_TREEDB_NODE_LINKED` / `EV_TREEDB_NODE_UNLINKED` whenever a hook-reachable relationship is created or destroyed, with the right parent/child/hook fields.

Companion to `c_node_link_events`, which covers the same behaviour at the `C_NODE` GClass level.

## Run

```bash
ctest -R test_tr_treedb_link_events --output-on-failure --test-dir build
```
