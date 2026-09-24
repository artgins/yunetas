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

4. **Failed open leaks no descriptor** (`test_failed_open_no_desc_leak`).
   Opening a schema with no `topics` list is rejected, and the shared
   `topic_cols_desc` the open path incref/creates before validation is released
   on the error path — the end-of-test memory check would otherwise report it as
   leaked. Before the fix the early error return skipped that decref.

5. **Ids and references** (`test_ids_and_refs`), on a treedb of its own:
   `owners` hooks `users` AND `groups` through a list hook and a dict hook.
   - A list hook holds the user `x` and the group `x`: two nodes. The bare-id
     membership test took the second for a duplicate.
   - A dict hook, keyed by the id alone, refuses the group `x` while it holds
     the user `x` (*"Cannot link, the dict hook holds a node of another topic
     with this id"*). It used to take the user's place.
   - An id of a topic with hooks that holds `^`, or is `NAME_MAX` long, is
     refused at create: every reference to the node would be undecodable, or
     cut in silence. A topic with no hooks keeps such an id.
   - A reference whose id part does not fit the decode is refused (*"Wrong
     reference: a part of it is too long"*), not cut.

## Run

```bash
ctest --test-dir build -R tr_treedb_hook_hygiene --output-on-failure
```

Editing `tr_treedb.c` requires reinstalling the lib and relinking the test
(static-lib relink trap): `cd kernel/c/timeranger2/build && make install`, then
remove the test binary and rebuild.
