# c_node_link_events test

Tests TreeDB link/unlink events at the `C_NODE` GClass level: whenever a node is linked or unlinked via a hook, the framework must publish `EV_TREEDB_NODE_LINKED` / `EV_TREEDB_NODE_UNLINKED` with the right payload and subscriber set.

It also checks `import-db` and `export-db`: the errors of an import counted by their cause (test 14); a link it cannot make (a parent that does not exist) counted as a `link failure`, with result `-1` (test 15); an import that aborts answering `-1` with `ABORTED` in its comment (test 16); and `export-db` with no `filename` naming its file with the integer `schema_version` (test 17, `treedb_link_test-1-<date>.trdb.json`, written under `~/tests_yuneta/c_node_link_events_realm/temp/` and removed). And `link-nodes` / `unlink-nodes` with a child ref split at its FIRST `^`: a user whose id holds `^` (`users^caret^user`) and one whose id is 255 bytes long are linked and unlinked by name (test 18). An `import-db` whose `content64` is not json answers `-1` and logs ONE WARNING *"frame is not json"* (`msgset` `Protocol`, no stack) that names the peer, `peername` `test-peer` of the gobj that sends the command (test 19; red before: an ERROR with a stack, naming no peer). `snap-content` reads only the topics of its treedb: a plain topic of the same tranger, with a record tagged with the snap number 7, is refused by name and left out of the overview (test 20; red before: both read it).

See also `tests/c/tr_treedb_link_events` for the lower-level (TreeDB-API) counterpart, and `CLAUDE.md` for the hook/fkey rules.

## Run

```bash
ctest -R test_c_node_link_events --output-on-failure --test-dir build
```
