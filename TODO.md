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

## Security: re-enable per-command authorization in command_parser

The `SDF_AUTHZ_X` / `gobj_user_has_authz` check at the command-dispatch
boundary in `kernel/c/gobj-c/src/command_parser.c:86-124` is **commented out**,
and has been since before the folder refactor (`299409d74`). A default global
authz checker is registered (`entry_point.c:85`) and would work if called, but
the only call site is disabled — so the 131 `SDF_AUTHZ_X` command entries carry
an authz flag that nothing reads.

Impact: **broken access control** — any *authenticated* principal can run any
`SDF_AUTHZ_X` command regardless of role/permission. This is NOT a remote-unauth
issue: authentication is enforced upstream (the `st_session` FSM gate plus the
explicit `authenticated` recheck at `c_ievent_srv.c:1492`).

Re-enabling is the canonical fix but is **not** a blind change — the role model
is data-driven (the `c_authz` treedb). Before re-arming the block:

1. Enumerate which roles/permissions the `SDF_AUTHZ_X` commands require.
2. Confirm internal `src`=self callers are granted (e.g. `c_mqtt_broker.c:4411`,
   `c_yuno.c:5305`) so internal command flows don't break.
3. Re-arm `command_parser.c:86-124`; do **not** substitute transport-layer authz
   (that proves identity, not permission).
4. Regression-test internal command paths after.

Found by a security review of gobj-c (finding f002, 2026-06-05). Note:
`gobj_user_has_authz` also fails open (`gobj.c:9451` returns TRUE when no checker
is registered) — keep in mind if the checker is ever deregistered.

## Security: ytls TLS posture (deployment decisions)

From a security review of `kernel/c/ytls` (2026-06-05). These are
configuration/posture choices, not memory-safety bugs — they need a policy
decision, so they're tracked here rather than blindly changed.

- **Protocol floor hardcoded to SSLv3/TLS1.0** (`src/tls/openssl.c:367`,
  `min_proto_version=0`). There is **no config knob** to raise it, so embedders
  can't opt into a modern floor. Decide a minimum (TLS1.2+ recommended) and
  optionally expose it via config; weigh against any legacy client that still
  needs old TLS.
- **mbedTLS `VERIFY_NONE` when no CA file is set** (`src/tls/mbedtls.c:332`);
  the server-with-CA path uses `VERIFY_OPTIONAL` with **no** `get_verify_result`
  check. Client connections without a configured CA therefore do not validate
  the peer certificate. Decide the desired default (fail-closed vs. opt-in) per
  client/server role.
- **TLS renegotiation left enabled** (`SSL_OP_NO_RENEGOTIATION` is commented
  out, openssl backend). Consider disabling unless a use case needs it.

(The memory-safety/integrity defects found in the same review — the
encrypt_data re-entrant UAF and the double `gbuffer_get` — were fixed
separately; see the `fix(ytls): re-entrant UAF in encrypt_data ...` commit.
The UAF's reachability — `SSL_write` returning WANT_* during egress while the
app frees the session from `on_clear_data_cb` — was not reproduced in a
deterministic test and would benefit from an execution-verified PoC.)

## Security: timeranger2 on-disk record hardening (contingent / local)

From a security review of `kernel/c/timeranger2` (2026-06-05). The on-disk
md2 record header (`__offset__`/`__size__`, big-endian) is read and used to
drive I/O. The dominant read sink (`read_record_content`) was hardened in the
`fix(timeranger2): validate on-disk md2 record offset/size ...` commit. Two
follow-ups remain, both **contingent on the on-disk topic files being a trust
boundary** — which is LOCAL only (replication is by hard-link, same host/fs)
and OFF by default in production (`rt_by_disk=FALSE` in `c_mqtt_broker`/`c_node`;
only the `emu_device` test yuno enables it):

- **`tranger2_delete_instance` zero-payload write loop** (`timeranger2.c:3404`)
  uses disk `__offset__`/`__size__` to drive a zeroing `write` into the data
  file with no span-within-own-segment guard — a forged header gives a
  cross-record overwrite/data-destruction primitive. Add the same
  offset+size-vs-filesize validation there (and ideally a same-segment check).
- **`fs_watcher.c:368` inotify parse loop** advances `ptr += sizeof(event) +
  event->len` with no remaining-bytes guard before dereferencing the next
  header/`event->name`. Kernel-framed input (low attacker reach), defense-in-depth.
- Also noted: `publish_new_rt_disk_records` (`timeranger2.c:4872`) doesn't check
  `read_md`'s return before using the record.

## Security: yev_loop — event UAF fix awaiting review

From a security review of `kernel/c/yev_loop` (2026-06-05). The DNS-parser
hardening (bounds in `static_resolv.c` + unpredictable transaction id) landed
on main. One finding is parked on a branch for review rather than merged:

- **F-005 use-after-free in `yev_destroy_event`** — destroying a still-running
  event freed the `yev_event` while its io_uring cancel/op CQE was still in
  flight; the completion later re-entered `callback_cqe` on freed memory
  (deref + indirect call through a freed `->callback`). The fix (in-flight CQE
  refcount + deferred free) is implemented on branch
  **`fix/yev-destroy-uaf`** (commit 394b00d5f), left UNMERGED: it touches the
  core reap path reached by every socket/file/timer op. It passes 25 event-loop
  tests with the mem-not-free check clean, but the UAF itself was not
  reproduced deterministically (needs an abrupt destroy-while-in-flight + ASAN).
  Review + decide whether to merge; see the commit message for the load-bearing
  edge case (stopping-but-still-running teardown).
- Lower-priority / by-design: the DNS source port is left to the OS ephemeral
  assignment (RFC 6056 randomized) rather than explicitly randomized — adequate,
  noted for completeness. `dns_parse_response` record-count/`rdlen` advances are
  bounded by the existing per-RR guards plus the F-001 decode fix.
