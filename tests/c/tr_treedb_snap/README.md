# `tr_treedb_snap` — snap mechanics regression

Covers the treedb snap primitives (`treedb_shoot_snap`,
`treedb_activate_snap`, `treedb_activate_snap("__clear__")`) and the
primary-index promotion that happens on the next `treedb_open_db`
reload — the same mechanic the agent uses on every release rotation.

## What it asserts

For a two-topic schema modelled after the agent's `binaries` +
`yunos` (both keyed by `id` with `pkey2s` for the per-version
secondary), it walks the full upgrade lifecycle and asserts the
primary version visible via `treedb_get_node` after each step:

| Phase | Action | Expected primary |
|------:|--------|------------------|
| 1 | seed v1 records | v1 |
| 2 | add v2 instances, no snap action | v1 (in-memory primary not promoted) |
| 3+4 | `shoot snap_v1` + `activate("__clear__")` + reload | v2 (newest pkey2) |
| 5 | add v3 + `activate("__clear__")` + reload | v3 |
| 6+7 | `shoot snap_v3` + `activate("snap_v1")` + reload | v1 (rollback) |
| 8 | `activate("snap_v3")` + reload | v3 |
| 9 | `activate("__clear__")` + reload | v3 (already latest, stays) |

The reload step (`treedb_close_db` + `treedb_open_db`) mirrors what
`c_agent::restart_nodes` does after toggling the snap flag — see
`yunos/c/yuno_agent/YUNO_LIFECYCLE.md` §6.5. Without it the
in-memory primary index does not move; the snap toggle alone is just
a flag on the `__snaps__` topic.

## What it does NOT cover

- The agent's `cmd_run_yuno` / `cmd_kill_yuno` lifecycle (no process
  launch / mock for `run_process2`).
- The SIGKILL-all step `restart_nodes` performs on existing live
  yunos. The test exercises only the data-layer snap mechanics that
  this kill step is wrapped around.
- Configuration topic. The schema has only `binaries` + `yunos`; the
  agent also has `configurations` keyed the same way, so the
  guarantees here propagate to it by construction.

## Run

```bash
cd tests/c/tr_treedb_snap/build && make && ctest --output-on-failure
```
