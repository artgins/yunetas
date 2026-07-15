# test_tr_treedb_hook_hygiene

Regression coverage for the treedb hook fixes in `tr_treedb.c` on a versioned
(pkey2) parent with a hook — reverse-hook unlink + idempotent link dedup, and
force-delete of a multi-child array hook.

The schema (`schema_sample.c`) models the agent's `configs`→`yunos` graph: a
**versioned** parent topic `configs` (pkey2 `version`) with a `yunos` hook, and
a child topic `yunos` whose `config` fkey points back. The child's fkey ref
stores only the config id, not the version — the asymmetry the fixes handle.

## Cases

1. **Idempotent link dedup** (`test_idempotent_link_dedup`). Linking the same
   child twice is a no-op: the parent hook and the child fkey each keep a single
   entry, and the skipped duplicate is surfaced via two `gobj_log_warning`s
   (parent hook, then child fkey) — asserted in strict FIFO order.

2. **Non-primary version unlink** (`test_clean_unlinks_nonprimary_version`).
   A child hooked on the **non-primary** config version is cleaned: the entry is
   removed from the version that actually held it (not the primary), the child
   fkey is cleared, and no *"Child data not found"* error is logged.

3. **Force-delete unlinks every array-hook child**
   (`test_force_delete_unlinks_all_array_children`). Force-deleting a `configs`
   node that hooks three `yunos` children unlinks **all** of them and removes
   the node. Before the fix the teardown iterated the hook array while
   `_unlink_nodes()` removed from it in place, so the index-based loop skipped
   the middle child; the re-check then found a leftover link and aborted the
   delete, leaving a half-unlinked graph.

## Run

```bash
ctest --test-dir build -R tr_treedb_hook_hygiene --output-on-failure
```

Editing `tr_treedb.c` requires reinstalling the lib and relinking the test
(static-lib relink trap): `cd kernel/c/timeranger2/build && make install`, then
remove the test binary and rebuild.
