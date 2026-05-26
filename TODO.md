# TODO

Tracks API renames, removals and additions between versions.

## Auth: OIDC migration follow-ups

The `c_auth_bff` (BFF) and `c_task_authenticate` (ROPC task) gclasses
were migrated from a Keycloak-only path scheme to standard OIDC
discovery + explicit endpoint override.

`c_task_authenticate` and its six callers (`c_cli`, `c_mqtt_tui`,
`c_ycommand`, `c_ystats`, `c_ytests`, `c_ybatch`) had their legacy
`auth_url` and `auth_system` attrs **removed** outright (no users
remained), and the `azp` attr was renamed to `client_id` to match the
form parameter actually sent on `/token` and `/logout`.

`c_auth_bff` still keeps its own `idp_url` + `realm` legacy pair,
now flagged with `SDF_DEPRECATED` so the framework emits the
standard "Deprecated attribute used in gobj creation" warning at
`gobj_create` time (one per attr).  Removal is scheduled once one
release has shipped with the warning in place.

### Smoke tests against real IdPs

Mocked tests cover the wire format; they do not cover quirks of
real IdP discovery documents.

- **Keycloak** — redeploy `auth_bff.1801.json` (already migrated
  to `issuer`) and verify login / refresh / logout work end-to-end
  against a real Keycloak realm.

- **Auth0 / Cognito / Authentik** — discovery document layouts
  differ subtly (Auth0 uses `/oauth/token`, Cognito uses
  `/oauth2/token`, Authentik uses `/application/o/token/`).
  Configure a test BFF against each and confirm the discovery
  response is parsed correctly and the follow-up token call
  succeeds. ROPC (`grant_type=password`) is **not** supported
  on most modern tenants of these IdPs by default — the BFF
  `/auth/callback` PKCE flow is the path that actually works
  cross-IdP; `c_task_authenticate` will fail there until ROPC
  is replaced (see below).

### Independent of OIDC migration

- **Replace ROPC in `c_task_authenticate`** — the task posts
  `grant_type=password` against the token endpoint. Discovery
  fixes the URL but does not fix the grant: Auth0, Cognito,
  Azure AD and Authentik either disable ROPC by default or
  refuse it for confidential clients. Migrating to PKCE
  (authorization code + code_verifier) would make the task
  work against any modern IdP, at the cost of needing a
  browser redirect — viable for `yuno_cli` and `ycommand`
  (which already prompt the user) but not for headless
  callers like `ybatch` / `ystats` / `ytests`.  Out of scope
  for the current migration; flagged here so it surfaces when
  the ROPC failure mode hits the first non-Keycloak deployment.

## tranger2: `tranger2_delete_instance` — DONE (2026-05-26)

### Vocabulary

timeranger2 has two delete granularities:

- A **record** is a primary key.  It lives in its own
  directory `keys/<key>/` with its own `.md2` index.
- An **instance** is one row in that key's `.md2` index,
  addressed by `(key, __t__, rowid)`.
  `tranger2_append_record(key, ..., __t__)` appends one
  instance (the name is historical; the contract is
  per-instance).

Both API entry points live in `timeranger2.h`:

- `tranger2_delete_key(tranger, topic, key)` — drop the whole
  `keys/<key>/` directory.
- `tranger2_delete_instance(tranger, topic, key, __t__, rowid,
  zero_payload)` — tombstone one row in place.

Both are irrecoverable; the distinction is granularity, not
reversibility.

### How `delete_instance` works

- The `.md2` row is mutated in place via the existing
  `get_md_record_for_wr` + `rewrite_md_to_file` plumbing.  OR
  `sf_deleted_instance = 0x0400` (re-introduced in `system_flag2_t`,
  inherited side of the mask so `rt_by_disk` followers see the same
  bit as the master) into the system_flag bits.
- The `.json` log stays append-only.  `rowid`s do **not**
  renumber — slot N stays slot N, just dark.
- If `zero_payload` is TRUE, overwrite the matching `__size__`
  bytes at `__offset__` in the data `.json` with `0x00`.  Opt-in
  because after the wipe the JSON file is no longer a parseable
  concatenation — only the `.md2` index makes sense of it.
- Master-only.  Second delete of the same instance is a silent
  no-op (returns 0, no log).

### Read paths that honour the bit

v7 read flow is funnelled through three sites — not the five v6
had, because v7 iteration is segment-based instead of
file-walking.  Each site does the same `is_deleted_instance()`
skip + `next_segment_row()` advance:

- `tranger2_open_iterator` history loop (`timeranger2.c`)
- `tranger2_iterator_get_page` (`timeranger2.c`)
- `publish_new_rt_disk_records` (`timeranger2.c`, rt_by_disk
  follower)

`tr_treedb.c` is **not** a fourth site: `load_id_callback` /
`load_pkey2_callback` are downstream callbacks of those three.
Filtering upstream covers treedb transparently — no tr_treedb
change.

`tranger2_read_record_content` and `tranger2_read_user_flag`
deliberately **do not** filter.  The caller supplies (key, __t__,
rowid) — the row is already located — so we still serve it for
audit / wipe-verification tooling.  This matches v6 behaviour.

### Known second-order effects (documented in the header)

- `iterator_size()`, `total_rows`, `pages` count slots, not live
  rows.  A page returning `data.length=2` with `total_rows=3` is
  the contract for "1 dead in this segment".
- `topic_cache` cells (`fr_t`/`to_t`/`fr_tm`/`to_tm`) are not
  refreshed.  If the deleted instance was the min/max t/tm of its
  file, the cell rollup may lie.  Cheap to fix on next cold
  reload; expensive to fix incrementally; deferred until a
  consumer needs it.

### Tests

`tests/c/timeranger2/test_delete_instance.c` — single binary,
five sub-cases: history skip + cold reload, paged skip +
`total_rows` over-report, `zero_payload` raw-byte check,
idempotent second delete, non-master refusal.

## tranger2: `delete_key` → subscriber propagation — DONE (2026-05-26)

### What was broken

`tranger2_delete_key()` used to `rmrdir(keys/<key>/)` and clear
the in-memory rollup cache, but never notified subscribers.
- `rt_mem` listeners (same process) kept stale references.
- `rt_by_disk` followers (other process) kept their cached view
  of the dead key alive — they only watched **appends** through
  the `disks/<rt_id>/` hardlink channel, never deletions.

Wattyzer's `db_history_wz` worked around this on every SPA
delete with a "tombstone-then-delete" recipe
(commits `04bde63` for tariffs, `3bc85da` for budgets in the
wattyzer repo): emit an `update_node` with a skeleton record
designed to look like "absent" downstream, then call
`delete_node`. The follower sees the tombstone fly by on its
append channel and prunes its cache.

### What landed

Master:
- New helper `mirror_key_delete_to_disks(topic, key)` walks
  `topic/disks/<rt_id>/` and `rmrdir`s every `<rt_id>/<key>/`
  it finds before the final `rmrdir(keys/<key>/)`. Followers
  watching their `disks/<rt_id>/` recursively pick this up as
  `FS_SUBDIR_DELETED_TYPE` on the standard inotify channel —
  no new IPC, no new file convention.
- New helper `fire_key_deleted_locally(topic, key)` iterates
  `topic.lists[]` (rt_mem), `topic.iterators[]` and
  `topic.disks[]`. Each entry's `key` filter is honoured (""
  matches any). Entries with an attached `fs_event_client`
  (active inotify watcher) are skipped here to avoid
  double-fire when the same process is both master and
  in-process follower.

Follower:
- `client_fs_callback`'s `FS_SUBDIR_DELETED_TYPE` branch — a
  v7 TODO that had been logging "NOT processed" since
  inception — now extracts the deleted key, clears
  `topic.cache[key]` and runs the same fan-out.
- `monitor_rt_disk_by_client` now stashes the `topic` json
  pointer in `fs_event->user_data2` so the callback can
  resolve the topic without walking the path.

API (additive, no breaking signature change):
- `typedef tranger2_key_deleted_callback_t`
- `PUBLIC tranger2_set_rt_key_deleted_callback(handle, cb, user_data)`
  registers on a handle returned by `tranger2_open_rt_mem`,
  `tranger2_open_rt_disk` or `tranger2_open_iterator`.

### Tests

`tests/c/timeranger2/test_delete_key_propagation.c` — single
binary, five sub-cases: rt_mem specific-key listener,
all-keys listener, filter skip, rt_disk in-process via
inotify, cache rollup cleared.

### Follow-up — remove the wattyzer tombstone recipe

The skeleton-then-delete dance in `wattyzer/.../db_history_wz`
becomes redundant once this lands in a deployed `yunetas`
release. Plan:

1. Ship yunetas with this change, deploy wattyzer against it.
2. Run **one production cycle** with both paths coexisting —
   confirm follower caches stay in sync on tariff and budget
   deletes without the skeleton update being load-bearing.
3. Remove the `delete_X_data` skeleton from `db_history_wz`
   (both tariff and budget paths), keep only the plain
   `gobj_delete_node`. Drop the matching agregador filter
   tricks that depended on the skeleton shape.

Out of scope here on purpose: not touching the wattyzer repo
in the same commit window — coexistence first, cleanup later.
