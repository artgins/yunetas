# TODO

Tracks API renames, removals and additions between versions.

## treedb: no durable per-instance (pkey2) node delete

**Root cause is in the treedb/timeranger2 layer, not the agent commands.**
`gobj_delete_node` -> `mt_delete_node` -> `treedb_delete_node` uses the pkey2
only to *locate* the node, then deletes by **key (id)**:
`treedb_delete_node` (tr_treedb.c:5088) calls `tranger2_delete_key(tranger,
topic, id)`, which `rmrdir`s the whole `keys/<id>` directory — *all* releases of
that id (timeranger2.c:2924-2960). The author flags this in-line:
`// TODO estoy borrando todas las instancias` (tr_treedb.c:5150), and
`tranger2_delete_record` is just `#define`d to `tranger2_delete_key`
(timeranger2.h:451). A proper per-instance primitive **already exists** —
`tranger2_delete_instance()` (timeranger2.h:481) — but `treedb_delete_node`
does not use it.

Observed (2026-06-03, agent treedb is master): deleting one yunos instance via
`command-agent ... delete-node record={"id":"5000","yuno_release":"1.4.6.0-3"}`
removed `-3` from the **in-memory** indexes only; it did **not** persist — after
an agent SIGKILL+reload `-3` came back from disk and, being the highest release,
was promoted to primary and the running yuno was reconciled onto it. So the
single-instance delete is non-durable, and routing the whole-key path as master
would instead nuke *every* release of the id.

Consequence: there is still no first-class, durable way to prune a single
superseded (or mistakenly-created **higher**) release/version short of a full
snap rollback. A higher-release instance left on disk is promoted on the next
`deactivate-snap` / agent reload.

Fix (treedb layer, core change — needs the kernel test matrix):
- give `treedb_delete_node` a per-instance path that, when the node carries a
  pkey2, calls `tranger2_delete_instance()` (durable single-record delete) and
  updates only that pkey2 index entry, instead of `tranger2_delete_key` +
  whole-id index teardown;
- then expose it through the agent: `delete-yuno`/`delete-config`/
  `delete-binary` (`cmd_delete_yuno` c_agent.c, `cmd_delete_config`,
  `cmd_delete_binary`) list via `gobj_list_instances` (not `gobj_list_nodes`)
  and forward the `pkey2` (`yuno_release` / `version`) when supplied, keeping the
  running-yuno and snap-tag guards. NB: instance records carry a stale
  `yuno_running` (the live flag lives on the in-memory primary), so the running
  guard must consult the primary, not the instance record.

An agent-only forward was prototyped and reverted: it *looks* like it deletes
the instance (the in-memory view updates) but rides the non-durable / whole-key
`treedb_delete_node` underneath, so it must not ship until the treedb layer is
fixed first.

## c_yuno: `priority` attr renamed to `sched_priority`

The `C_YUNO` attr that feeds `sched_setscheduler` (default `20`, only
applied when `cpu_core > 0`) was renamed `priority` -> `sched_priority`
so the name no longer collides with the two *other* "priority" planes:
the per-service start order (0..9, `manage_services.c`) and the agent's
per-yuno `start_priority`. Only `c_yuno.c` (the SDATA + its single
reader) and the agent's config injection (`build_yuno_running_script`)
were touched; the service-level `priority` field is unchanged.

Backwards-compat: the attr is `SDF_PERSIST`. A yuno store that persisted
a non-default `priority` will fall back to the `sched_priority` default
(20) after upgrade, and a yuno's own external config that sets `priority`
is now ignored. Both are no-ops in practice (the value is only consulted
when `cpu_core > 0`, which no shipped yuno sets), so no migration shim
was added. Set the value the node-local way instead: the agent's
`sched_priority` column (see YUNO_LIFECYCLE.md §4.8).

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
  browser redirect — viable for `ycli` and `ycommand`
  (which already prompt the user) but not for headless
  callers like `ybatch` / `ystats` / `ytests`.  Out of scope
  for the current migration; flagged here so it surfaces when
  the ROPC failure mode hits the first non-Keycloak deployment.

## emu_device: runtime-validate the replay path

`emu_device`'s frame-emission was ported from the removed timeranger v1
API to v7 (`tranger2_open_list` + collect callback; output side built in
code like `sgateway`; `window`/`interval` emission of base64 `frame64`
records). It is **compile-verified only** — not yet runtime-tested,
because validating it needs a timeranger2 topic whose records carry a
base64 `frame64` field plus a TCP sink, neither of which was available.

When a fixture exists, verify end-to-end:

- Point `emu_device` at a `frame64` topic (`path`/`database`/`topic`) and
  a TCP sink (`url`, e.g. `nc -lk <port>`), with `window`/`interval` set.
- Confirm on connect it sends the `leading` frame then `window` frames per
  `interval`; `read-parameters` shows "frames sent" advancing and "frames
  loaded" = the number matched.
- Specifically check: `tranger2_open_list` collects via the load callback,
  the output side connects and emits, and `mt_pause` cleans up with no
  gbmem leak (run a start/stop cycle for the shutdown audit).

Note: `open_list` loads all matching records into memory (the v1 path was a
lazy cursor) — point it at a bounded range for large topics.
