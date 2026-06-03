# TODO

Tracks API renames, removals and additions between versions.

## agent: cannot delete a non-primary yuno/config/binary instance

`delete-yuno`, `delete-config` and `delete-binary` (`cmd_delete_yuno`
c_agent.c:4749, `cmd_delete_config` :3913, `cmd_delete_binary` :3470) all
resolve their target with `gobj_list_nodes(...)`, which returns **one primary
node per id** — the in-memory primary, not the per-`pkey2` instance. So a filter
like `delete-yuno id=5000 yuno_release=1.4.6.0-3` (or `delete-config
id=db_tracks.5000 version=3`) either matches the *primary* release/version or
nothing at all (`"Select some ... please"`). `cmd_delete_yuno` even hardcodes
`json_pack("{s:s}", "id", ...)` (c_agent.c:4839), dropping the `pkey2` entirely
before calling `gobj_delete_node`.

Consequence: there is no first-class way to prune a single superseded (or
mistakenly-created **higher**) release/version of a yuno without a full snap
rollback. This bites specifically when an instance whose `yuno_release` sorts
*above* the running primary needs to be removed — left in place,
`promote_highest_release_yunos` / a treedb reload would promote it on the next
`deactivate-snap` or agent restart.

The underlying treedb already supports it: `C_NODE`'s `delete-node`
(`cmd_delete_node` / `mt_delete_node`, c_node.c) finds the node by `id` **plus**
the `pkey2` field(s). The current operator workaround is to bypass the agent
commands and drive the treedb directly:

```
ycommand -c 'command-agent service=treedb_yuneta_agent command=delete-node \
    topic_name=yunos record={"id":"5000","yuno_release":"1.4.6.0-3"} \
    options={"force":1}'
```

Note the `record` JSON must be **double-quoted** — the value reaches the treedb
parser as a wild string and is coerced via `anystring2json`
(command_parser.c:534), which rejects single-quoted JSON, so single quotes
silently drop the field (→ `"field 'id' is required"`).

Fix: have `delete-yuno`/`delete-config`/`delete-binary` forward the `pkey2`
(`yuno_release` / `version`) into `gobj_delete_node` when supplied — list via
`gobj_list_instances` (not `gobj_list_nodes`) so a specific instance can be
selected and deleted, falling back to today's primary behaviour when no `pkey2`
is given. Keep the running-yuno and snap-tag guards.

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
