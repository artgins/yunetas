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

## tranger2: add `tranger2_delete_instance` (v6 → v7)

### Vocabulary

timeranger2 has two delete granularities:

- A **record** is a primary key.  It lives in its own
  directory `keys/<key>/` with its own `.md2` index.
- An **instance** is one row in that key's `.md2` index,
  addressed by `(key, __t__, rowid)`.
  `tranger2_append_record(key, ..., __t__)` appends one
  instance (the name is historical; the contract is
  per-instance).

v7 has the record-level delete: **`tranger2_delete_key()`**
(renamed from `tranger2_delete_record` on 2026-05-25; legacy
alias kept as `#define` in `timeranger2.h`).  What is missing
is per-instance delete.  Both deletes are irrecoverable; the
distinction is granularity, not reversibility.

### What was removed and what remains

v6 had a per-instance delete under flag `sf0_deleted_record`.
The matching v7 flag — `sf_deleted_record` at bit `0x0400` — was
removed in commit `eb2c454a7` ("remove flag sf_deleted_record",
2026-05-11) because nothing in v7 was using it.  The slot
in `system_flag2_t` is free (sits between `sf_tm_ms = 0x0200`
and `sf_loading_from_disk = 0x1000`) and its slot in
`sf_names[]` carries `""`.

No in-tree consumer today.  `treedb_delete_instance()` (a sibling
of `treedb_delete_node()`) handles per-`pkey2`-index cleanup
without touching the on-disk row, so the treedb side does NOT
need `tranger2_delete_instance`.  Expected first callers are
external: housekeeping jobs (selective compaction of historical
appends), GDPR / sensitive-payload wipes, test fixtures.

### Plan (when a real consumer needs it)

The log files stay **append-only** — deletion does not
rewrite the log.  The per-key `.md2` **index is mutable**
(the existing `tranger2_write_user_flag()` already mutates
`md2_record_t` rows in place — same plumbing).

- **Re-introduce the flag** as **`sf_deleted_instance = 0x0400`**
  (clearer than the old `sf_deleted_record` — the bit lives on
  one instance, not on the key as a whole).  Add the matching
  entry in `sf_names[]`.  Bit position stays inherited so the
  `rt_by_disk` followers see the same flag the master does.
- **Add `tranger2_delete_instance(tranger, topic_name, key,
  __t__, rowid, zero_payload)`**.  Internally:
  - `get_md_record_for_wr()` to locate the slot;
  - OR `sf_deleted_instance` into `system_flag`, `pwrite` 32
    bytes back;
  - update the matching entry in `topic_cache`;
  - if `zero_payload`: `lseek(__offset__)` + write `__size__`
    bytes of zeros in `data/<mask>.json` (v6 did this
    unconditionally; we keep it opt-in for sensitive-data wipes).
- **Honor the bit on read** in the 4-5 entry points that
  iterate `.md2` rows (`tranger2_open_iterator`,
  `tranger2_iterator_get_page`, the internal
  next/prev helpers, the `rt_by_disk` follower callback at
  `/disks/<rt_id>/`, and `treedb_load_topic_records` in
  `tr_treedb.c`).  v6 has the canonical 5-site pattern
  preserved in `utils/c/tr2migrate/30_timeranger.c`
  (search for `sf0_deleted_record` — 5 hits in the read
  paths).
- **Tests** under `tests/c/timeranger2`: create 3 instances of
  one key, mark instance #2 deleted, expect 2 readable; same
  with a cold reload; same with an `rt_by_disk` follower.

### Operator workaround until then

Cosmetic only: re-append the same key with a sentinel value /
cleanup-tagged source so the last-wins-per-key projection
hides the original instance.  Full wipe of a record (key +
all instances) is `tranger2_delete_key()`.  No per-instance
wipe until the above lands.
