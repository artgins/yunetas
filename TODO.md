# TODO

Tracks API renames/removals/additions between versions and the still-open
follow-ups of the 2026-06 security-hardening work.

Everything already shipped is in [`CHANGELOG.md`](CHANGELOG.md) (the release
notes); the design/rationale for shipped features lives in the docs
(`yunos/c/yuno_agent/YUNO_AUTH.md`, `docs/doc.yuneta.io/yunos/mqtt_broker.md`,
`docs/doc.yuneta.io/guide/guide_tls.md`) and the full narrative in git history.

> The 2026-06 security batch shipped across **7.5.2 → 7.5.4**: memory-safety +
> injection hardening (gobj-c / root-linux / ytls / yev_loop / timeranger2 /
> libjwt / modbus / dba_postgres / emailsender / mqtt); the per-command authz
> gate (`enable_command_authz`, default off); the MQTT publish + subscribe ACL
> (model A, default off); secure-by-default TLS; and the `emu_device` fix + move
> to `utils/c/`. Only the open follow-ups remain below.

## Auth: OIDC migration follow-ups

- **Smoke tests against real IdPs.** Mocked tests cover the wire format only.
  Keycloak: redeploy `auth_bff.1801.json` (migrated to `issuer`) and verify
  login / refresh / logout end-to-end. Auth0 / Cognito / Authentik: discovery
  layouts differ (`/oauth/token`, `/oauth2/token`, `/application/o/token/`) —
  confirm discovery parses and the token call succeeds against each.
- **Replace ROPC in `c_task_authenticate`** with PKCE (authorization code +
  code_verifier). Auth0 / Cognito / Azure AD / Authentik disable ROPC by
  default, so `grant_type=password` fails there. Viable for `ycli` / `ycommand`
  (they prompt); needs another path for headless `ybatch` / `ystats` / `ytests`.
- **Remove the deprecated `idp_url` + `realm` pair** from `c_auth_bff` (flagged
  `SDF_DEPRECATED` since 7.5.x) now that a release has shipped with the warning.

## Security: per-command authz gate — production enablement

The gate (`enable_command_authz`) shipped and was redesigned + validated in
7.5.4 (external-only check + global-authz resolution; design in YUNO_AUTH.md
§4.5). It is **default-off**. To enable in production, per node:

- provision **every principal that sends commands TO each C_AUTHZ yuno** with
  `__execute_command__`/root — not only `yuneta`/admins but the **controlcenter**
  user(s) that reach the agent's `:1993` port (the agent store currently has
  `yuneta` + `yuneta_admin@…` + `yunetas_admin@…`, NOT `yuneta_agent@…`);
- confirm each of the 5 C_AUTHZ stores (agent, agent22[shared], controlcenter,
  mqtt_broker, emailsender) holds the `root`/`yuneta` model at runtime — re-seed
  via `update-node` if a store was non-empty and missed `Authz.initial_load`;
- run a real **low-privilege deny test** on staging (needs a non-root external
  principal — infeasible on the yuneta-only local plano);
- then set `enable_command_authz: true` per yuno (pilot the agent first),
  staging → production.

Event-level authz (`EVF_AUTHZ_INJECT` / `EVF_AUTHZ_SUBSCRIBE`) is still
**declared but not enforced** — no gate exists for `gobj_send_event` /
`gobj_subscribe_event`.

## Security: ytls TLS posture — per-gate rollout

The secure-by-default flips (TLS1.2 floor, renegotiation disabled,
OpenSSL/mbedTLS peer verification with computed defaults) shipped — see
guide_tls.md. Each downgrade/rejection is logged so reduced-security gates are
enumerable. Remaining is **per-gate deployment config** (validate on staging):

- raise high-level gates explicitly where wanted; set the IoT-compat profile
  (`ssl_min_version` + `ssl_ciphers "@SECLEVEL=0"`, OpenSSL backend) on legacy
  gates;
- turn on peer verification per high-level gate (`ssl_trusted_certificate` or
  `ssl_use_system_ca`); IoT gates stay `none`/`optional`. The no-CA→`NONE`
  client default is logged ("TLS client WITHOUT server-certificate validation")
  — close those gaps per gate.

## Security: MQTT broker ACL — model + default-deny decision

The publish + subscribe ACL (model A: per-group `publish_acl`/`subscribe_acl`
in the broker treedb, `enable_acl` default off) shipped in 7.5.3 / 7.5.4 — see
mqtt_broker.md. Open decisions (Rosa):

- the **A/B/C model choice** — A = treedb group ACL (shipped); B = reuse the
  framework `C_AUTHZ` via `gobj_user_has_authz` (one authz system, but its
  checker is per-authz-name not topic-pattern → needs extending); C = a broker
  config attr holding the pattern map (no schema migration, off the treedb/UI);
- whether to flip enforcement to **default-deny** (validate on staging first).

## Security: not yet reviewed for memory-safety

- `modules/postgres` (libpq wrapper) — delegated to libpq, lower priority.
- the `yuno_agent` control plane + `watchfs` command-exec — re-audit if the
  agent's `SDF_WR` command attrs become remote-writable (prior fixes
  `8c03eb686` / `5dbede6a1` + authz gating).

## Security: vendored libjwt — maintenance

- **Backport `jwks_*` keyring NULL-safety** (`jwks_item_get(NULL)` /
  `jwks_free(NULL)`) at the next re-vendor — the vendored v3.2.1+2 copy derefs
  `jwk_set->head` (`jwks.c:201`). Low-sev: not reachable from `c_authz` (keyring
  always valid); the regression test documents and skips it.
- **Periodic re-vendor from upstream** — procedure in
  `kernel/c/libjwt/README.md` (§ Re-vendor procedure).

## emu_device: replay path — minor follow-up

Shipped in 7.5.3 (frame-emission fix + move to `utils/c/`). Open (LOW): in
standalone CLI mode `finish_replay()` calls `exit(0)` right after the last send,
which can truncate the final TCP flush (harmless in agent-managed mode — the
branch is skipped when `agent_client` exists). Defer the exit one event-loop
tick or wait for tx-drain.
