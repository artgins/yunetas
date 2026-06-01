# Authentication, authorisation, and TLS

This document covers the three intertwined operational concerns nobody can
afford to misunderstand on a Yuneta host: **who is calling** (authn),
**what they're allowed to do** (authz), and **the TLS that protects the
wire**.

Sibling to [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md), [`DEBUGGING.md`](DEBUGGING.md),
[`IPC.md`](IPC.md), [`REALMS.md`](REALMS.md), [`SCAFFOLDING.md`](SCAFFOLDING.md).

> ⚠️ **Read §4.5 and §8.3 before assuming anything about authz enforcement.**
> The per-command authz check is currently **commented out** in the framework
> (`kernel/c/gobj-c/src/command_parser.c:73-113`). Every `pm_*` schema you
> see on commands is **decorative**: declared, present in the binary,
> never consulted. Treat commands as authenticated-but-not-authorised
> until that block is uncommented.

---

## 1. Mental model

Three independent pieces, often confused:

```
  ┌─────────────────────────────────────────────────────────────────┐
  │  authentication = "who is calling"                              │
  │     (the auth_bff yuno + Keycloak + JWT in an HttpOnly cookie)  │
  └────────────────────────────────────────────────┬────────────────┘
                                                   │
                                                   ▼
  ┌─────────────────────────────────────────────────────────────────┐
  │  authorisation = "is the caller allowed to do X"                │
  │     (C_AUTHZ gclass + authzs treedb + pm_* schemas)             │
  │     ⚠️  Per-command check is COMMENTED OUT (see §4.5, §8.3)     │
  └────────────────────────────────────────────────┬────────────────┘
                                                   │
                                                   ▼
  ┌─────────────────────────────────────────────────────────────────┐
  │  TLS = "the bytes on the wire are confidential"                 │
  │     (ytls + cert_sync_* on the agent + reload-certs broadcast)  │
  └─────────────────────────────────────────────────────────────────┘
```

End-to-end request flow on a real production yuno:

```
   browser SPA
        │
        │  1. POST /auth/login  →  auth_bff  →  Keycloak  → tokens
        │  2. Set-Cookie: access_token (HttpOnly, Secure, SameSite=Strict,
        │     Domain=hostname.tld)
        │
        ▼
   WS upgrade to backend yuno (any port on same Domain)
        │ Cookie header carries access_token
        │
        ▼
   C_PROT_WEBSOCKET  →  C_IEVENT_SRV
        │
        │  3. C_IEVENT_SRV pulls the cookie, hands it to C_AUTHZ
        │
        ▼
   C_AUTHZ
        │
        │  4. libjwt verifies signature (JWKS)
        │     + checks claims (issuer, azp/client_id, exp)
        │  5. extracts username → writes __username__ to the source gobj
        │
        ▼
   command dispatch (gobj_command)
        │
        │  6. ⚠️ The pm_* check that should fire here is COMMENTED OUT.
        │     The handler runs whether or not the user has the authz.
        │
        ▼
   the handler (cmd_run_yuno, cmd_list_yunos, …)
```

---

## 2. The `auth_bff` yuno — authentication

A standalone Yuneta yuno that runs the `C_AUTH_BFF` kernel gclass. It is
the **only** thing on the system that talks OAuth2 to the IdP. The SPA
never sees a token — it just carries the cookie.

### 2.1 Why a BFF (and not the SPA talking to Keycloak)

Tokens live in **HttpOnly cookies**, scoped by domain (no port). JavaScript
cannot read them; XSS attacks cannot exfiltrate them. The SPA only knows
"am I authenticated" by the response code of API calls. This is the
SEC-04/-06/-07/-09 hardening Yuneta deployments require.

### 2.2 The four endpoints

Implemented in `kernel/c/root-linux/src/c_auth_bff.c`. URL dispatcher at
`c_auth_bff.c:2110-2236`.

| Endpoint           | Method | Purpose                                              | Sets cookies?       |
|--------------------|--------|------------------------------------------------------|---------------------|
| `/auth/login`      | POST   | Username/password (Resource Owner Password Credentials grant) | yes              |
| `/auth/callback`   | POST   | PKCE code exchange (authorisation_code grant)        | yes                 |
| `/auth/refresh`    | POST   | Reads refresh_token cookie, gets new access_token    | yes                 |
| `/auth/logout`     | POST   | Calls IdP `end_session_endpoint`, clears cookies     | yes (Max-Age=0)     |
| `OPTIONS *`        | OPTIONS | CORS preflight                                       | no                  |

### 2.3 PKCE authorisation-code flow

`c_auth_bff.c:2135-2179, 1383-1476`. The flow:

1. SPA generates `code_verifier`, derives `code_challenge`, redirects to
   IdP `/auth` with the challenge.
2. IdP redirects back with `code`.
3. SPA POSTs `{code, code_verifier, redirect_uri}` to `/auth/callback`.
4. BFF validates `redirect_uri` against `allowed_redirect_uri`
   (`c_auth_bff.c:2160-2167`, SEC-06).
5. BFF calls IdP `/token` with `grant_type=authorization_code` +
   `code_verifier` (`c_auth_bff.c:1431-1446`).
6. Tokens come back. BFF writes them as HttpOnly cookies
   (`c_auth_bff.c:1281-1336`).

State and nonce are the SPA's responsibility — the BFF does not generate
them.

### 2.4 The cookies

Built in `make_set_cookie()` at `c_auth_bff.c:748-762`:

```
Set-Cookie: access_token=<jwt>; Max-Age=<expires_in>;
            Path=/; HttpOnly; Secure; SameSite=Strict; Domain=<host>
```

- `HttpOnly` (`c_auth_bff.c:753`) — JS cannot read.
- `Secure` — HTTPS only.
- `SameSite=Strict` — no cross-site CSRF.
- `Path=/` — sent with all requests to the origin.
- `Domain` (`c_auth_bff.c:756`) — set from `cookie_domain` attr, no port.
  Means a cookie set by the BFF on port 1801 is automatically sent with
  the WebSocket upgrade to ports 1600 / 1800 / etc on the same hostname.
- `Max-Age` — `expires_in` for access, `refresh_expires_in` for refresh.

Logout clears both with `Max-Age=0` (`c_auth_bff.c:767-770`).

### 2.5 The OIDC config: `issuer` vs deprecated `idp_url` + `realm`

`attrs_table` at `c_auth_bff.c:181-192`:

| Attribute              | Status              | Purpose                                              |
|------------------------|---------------------|------------------------------------------------------|
| `issuer`               | preferred           | OIDC issuer URL, e.g. `https://auth.example.com/realms/foo/`. Triggers discovery via `/.well-known/openid-configuration`. |
| `token_endpoint`       | explicit override   | Bypass discovery; force the token URL.               |
| `end_session_endpoint` | explicit override   | Same, for logout.                                    |
| `idp_url`              | **`SDF_DEPRECATED`** | Legacy Keycloak base URL.                            |
| `realm`                | **`SDF_DEPRECATED`** | Legacy Keycloak realm name.                          |
| `client_id`            | required            | OAuth2 client id. Also the value the JWT's `azp` claim must match. |
| `client_secret`        | optional            | Empty for public clients with PKCE.                  |
| `redirect_uri`         | per-request         | From the callback request body.                      |
| `allowed_redirect_uri` | required            | Allow-list prefix for `/auth/callback` redirect_uri. |
| `cookie_domain`        | required            | Domain attribute for cookies (no port).              |

Legacy fallback: if `idp_url` + `realm` are present and `issuer` is not,
the code constructs the legacy URL as
`<idp_url>/realms/<realm>/protocol/openid-connect` (`c_auth_bff.c:358`)
and emits a deprecation warning.

The 2026-04-30 migration unified everything under `issuer` + (optional)
explicit endpoints. New deployments should not set `idp_url` / `realm`.

### 2.6 Per-host runtime configuration

`yunos/c/auth_bff/batches/<host>/auth_bff.<port>.json`. The shape is
illustrated by the localhost dev example (`batches/localhost/auth_bff.1801.json:55-58`):

```json
"issuer":        "https://auth.artgins.com/realms/yunetas.com/",
"client_id":     "treedb.yunetas.com",
"client_secret": "",
"cookie_domain": ""
```

In production deployments those four come from the project's Keycloak
realm. See §7 for the project conventions.

### 2.7 Pending bugs

Two issues are tracked but not fixed (per
`project_auth_bff_pending_bugs`):

- **HTTP_CL chain leak** (`c_auth_bff.c:387-413, 1910-1978`). Under rapid
  browser-disconnect during a `/token` call, the outbound
  `C_PROT_HTTP_CL` chain to Keycloak isn't always reclaimed cleanly.
- **No real-IdP smoke tests.** The auth_bff test suite at
  `tests/c/c_auth_bff/` runs against `c_mock_keycloak.c` only. Live
  Keycloak regressions are caught manually.

---

## 3. JWT validation on incoming requests

### 3.1 The Cookie crossing the WS upgrade

The browser's WebSocket upgrade request includes the cookies set by the
BFF (same Domain). `C_PROT_HTTP_SR` parses the headers, and the resulting
gobj tree carries the `Cookie` header through the upgrade into
`C_IEVENT_SRV` (or `C_AUTHZ` acting as the external gate).

### 3.2 Reading the JWT

`c_ievent_srv.c:56-70` declares two volatile attributes the channel
exposes after auth:

- `http_cookie` — the raw cookie header (set by `c_authz` during upgrade).
- `jwt_payload` — the decoded JWT payload, also set by `c_authz`.

The comment at `c_ievent_srv.c:55` is explicit: *"HACK set by c_authz,
this gclass is an external entry gate!"*. The actual cookie→JWT path
runs **inside `C_AUTHZ`**, not `C_IEVENT_SRV`.

### 3.3 Signature verification: libjwt

`kernel/c/libjwt/` — Yuneta vendors a copy of libjwt. The verification
entry point is `jwt_parse()` in `jwt-verify.c:83`. Keys come from JWKS
fetched from the issuer (cached, refreshed on rotation). The crypto
backend is OpenSSL or mbedTLS, runtime-selectable via the same `ytls`
abstraction used by TCP.

### 3.4 Claim validation: the `azp` → `client_id` migration

The JWT's `azp` (authorized party) claim must match the configured
`client_id`. Per `c_task_authenticate.c:196`:

```
"OAuth2 client_id (Keycloak/Auth0/Azure AD/...).
 Sent verbatim as the client_id form parameter on /token and /logout.
 Also matches the JWT 'azp' claim"
```

Before the 2026-04-30 migration the check string was hard-coded as
`"azp"`; after the migration the BFF reads the configured `client_id`
and validates against it. New deployments should rely on the configured
attribute, not on the literal `azp` name.

Other validated claims: `iss` (must match `issuer`), `exp` (expiry),
`nbf` if present.

### 3.5 The `__username__` attribute

After successful authn, `c_authz.c:945, 969, 1177` writes the resolved
username into the source gobj's `__username__` attribute:

```c
gobj_write_str_attr(src, "__username__", username);
```

Every later authz check pulls `__username__` from there. Code calling
into the framework on behalf of a user can populate this attribute
manually for test fixtures; in production it always comes from a JWT.

---

## 4. Authorisation: `C_AUTHZ`

The `C_AUTHZ` gclass (`kernel/c/root-linux/src/c_authz.c`, 4114 lines)
is the singleton authorisation service. One instance per yuno (created
as the default `authz` service in the `yuno_citizen` template, see
[`SCAFFOLDING.md`](SCAFFOLDING.md) §5.1). Other gobjs find it with
`gobj_find_service_by_gclass(C_AUTHZ, TRUE)` (`c_authz.c:4011, 4099`).

### 4.1 The `authzs` treedb schema

`kernel/c/root-linux/src/treedb_schema_authzs.c:58-343`. Three topics:

| Topic           | pkey        | Notable columns                                                       |
|-----------------|-------------|-----------------------------------------------------------------------|
| `users`         | `id`        | `disabled`, `max_sessions`, `credentials`, `properties`, `__sessions__`, `roles[]` (fkey to roles) |
| `roles`         | `id`        | `parent_role_id` (fkey for inheritance), `service`, `permission`, `permissions[]`, `deny`, `parameters`, `users{}` (dict hook back to users) |
| `users_accesses`| `id`+`tm`   | login audit: `ev`, `ip`, `jwt_payload`                                |

Roles can inherit from a parent (`parent_role_id`) — `get_user_roles()`
at `c_authz.c:3254-3266` walks the chain and accumulates effective
authzs.

### 4.2 The `yuneta` super-user

`c_authz.c:796-815`:

```c
if(strcmp(username, "yuneta") != 0) {
    gobj_log_warning(…, "Without JWT/passw only yuneta is allowed", …);
    return json_pack(…, "comment", "Without JWT/passw only yuneta is allowed", …);
}
```

`yuneta` is the only user permitted to authenticate **without** a JWT
or password. This is the authentication-side bypass — there is **no
matching authz bypass**. The agent's `__username__` attribute defaulting
to `"yuneta"` (`c_agent.c:914`) gives the agent itself this bypass for
its local CLI calls.

If a check is enforced (see §4.5), `yuneta` does *not* automatically
pass. The authz check is a separate lookup; `yuneta` happens to typically
own every role in production deployments.

### 4.3 `gobj_user_has_authz`

The predicate. `gobj.h:1607-1611`, body at `gobj.c:9400-9452`:

```c
PUBLIC BOOL gobj_user_has_authz(hgobj gobj, const char *authz, json_t *kw, hgobj src);
```

Resolution order:

1. The gclass's own `mt_authz_checker` method, if declared
   (`gobj.c:9423-9433`).
2. The globally-installed `__global_authorization_checker_fn__`
   (`gobj.c:9438-9448`). This is set by `C_AUTHZ` at registration.
3. If neither is installed, **returns `TRUE`** (default-allow).

That last point matters: a yuno with no `C_AUTHZ` service running has no
authz enforcement at all. Every call passes.

### 4.4 The `pm_*` and `SDATAAUTHZ` schemas

`gobj.h:218-263`. Two macros define the schema:

- `SDATAPM(type, name, flag, default, description)` — a parameter row.
- `SDATAAUTHZ(...)` — declares an authz that a command requires (with
  optional alias and items schema).

A command's parameter schema is declared once as a `sdata_desc_t` array
and referenced in the `SDATACM2` row in the command table. Example from
`c_agent.c:405-413`:

```c
PRIVATE sdata_desc_t pm_run_yuno[] = {
    SDATAPM(DTP_STRING, "id",        0, 0, "Id of yuno"),
    SDATAPM(DTP_STRING, "realm_id",  0, 0, "Realm Id"),
    SDATAPM(DTP_STRING, "yuno_role", 0, 0, "Yuno Role"),
    …
};
```

The framework treats `pm_run_yuno` as a parameter schema for input
validation (which **is** enforced). The authz-flag handling on commands
is the part that is commented out (next section).

### 4.5 ⚠️ The command authz check is commented out

`kernel/c/gobj-c/src/command_parser.c:71-113`:

```c
/*-----------------------------------------------*
 *  Check AUTHZ
 *-----------------------------------------------*/
//     if(cnf_cmd->flag & SDF_AUTHZ_X) {
//         json_t *kw_authz = json_pack("{s:s}", "command", command);
//         …
//         if(!gobj_user_has_authz(gobj, "__execute_command__", kw_authz, src)) {
//             …
//             return msg_iev_build_response(…, -403, "No permission to execute command", …);
//         }
//     }
```

The entire block is **commented**. Effects:

- The `SDF_AUTHZ_X` flag on a command is parsed but never read.
- `gobj_user_has_authz("__execute_command__", …)` is never invoked from
  the command dispatcher.
- Every authenticated caller can execute every command on every gobj.

The other places that *do* invoke `gobj_user_has_authz` (custom code in
specific gclasses) still work, but there is no framework-level
enforcement for commands.

When this is uncommented, every existing `pm_*` schema starts mattering.
Production deployments that have not exercised role assignments yet will
suddenly see denials. Treat it as a coming breaking change.

### 4.6 `EVF_AUTHZ_INJECT` / `EVF_AUTHZ_SUBSCRIBE`

`gobj.h:332-333` declares the flags; `gobj.c:418-419` declares the
matching global authzs (`__inject_event__`, `__subscribe_event__`). The
**enforcement** for these flags is not found in the dispatcher
(`gobj_send_event`, `gobj_subscribe_event`). Same status as the command
check: declared, not enforced.

### 4.7 Where authz **is** actually enforced today

Custom code inside specific gclasses that calls `gobj_user_has_authz`
directly. Examples worth knowing about:

- Inside `C_AUTHZ`'s own commands (you cannot list users without
  authority over the auth service itself).
- Inside `auth_bff` for things like cookie-domain validation
  (`c_auth_bff.c` Host-vs-Domain matching, SEC-06).

For any user-facing service that exposes commands and needs them
authorised right now, **add an explicit `gobj_user_has_authz` call at
the top of each command handler**. Don't rely on the framework flag.

### 4.8 Per-instance config keys (`authz.*`)

The `C_AUTHZ` gclass reads a small set of attrs at boot (see
`c_authz.c:295-310` `attrs_table`):

| Key                       | Status                | Purpose                                                                                  |
|---------------------------|-----------------------|------------------------------------------------------------------------------------------|
| `authz.master`            | bool                  | Whether this instance owns the authz treedb (writer) or follows another (reader).        |
| `authz.authz_service`     | preferred             | Service name under which to build/look up the authz tree. Empty → defaults to `yuno_role`. |
| `authz.authz_yuno_role`   | **`SDF_DEPRECATED`**  | Legacy alias for `authz.authz_service`. Fallback at `c_authz.c:417` — only read if `authz_service` is empty. New configs must use `authz.authz_service`. |
| `authz.tranger_path`      | optional              | External tranger storage path (when sharing the authz treedb across instances).          |

Same `Authz.*` keys (capital A) appear in some legacy configs — both
spellings are accepted by jansson's path resolution, but the canonical
form is the lowercase `authz.*` used in `yuno_agent/src/main.c:97`.

There is also a JWKS migration analogous to §2.5:

| Key                       | Status                | Purpose                                                          |
|---------------------------|-----------------------|------------------------------------------------------------------|
| `Authz.jwks`              | preferred             | Array of full JWK objects (the standard format).                  |
| `Authz.jwt_public_keys`   | legacy                | Older `iss` + `pkey` (raw PEM) tuple. Superseded by `Authz.jwks`. Drop from new configs. |

**Gotcha:** if you use the deprecated `authz.authz_yuno_role`, the
controlcenter will silently reject the agent's identity card ("User not
exist") — the JWT validates fine but the user→service mapping returns
empty. Both spellings reach `c_authz.c` but the deprecated one
generally lags behind in coverage. Always prefer `authz.authz_service`.

---

## 5. `C_AUTHZ` commands (user / role CRUD)

Declared in the `command_table` at `c_authz.c:260-282`. Just the names:

| Command            | Purpose                                                       |
|--------------------|---------------------------------------------------------------|
| `help`             | List commands                                                 |
| `authzs`           | Authz help                                                    |
| `list-jwk`         | JWKS keys cached by libjwt                                    |
| `add-jwk`          | Add a JWK manually                                            |
| `remove-jwk`       | Remove a JWK                                                  |
| `users`            | List users                                                    |
| `accesses`         | List `users_accesses` audit rows                              |
| `create-user`      | Create a user row                                             |
| `enable-user`      | Flip `disabled=false`                                         |
| `disable-user`     | Flip `disabled=true`                                          |
| `delete-user`      | Remove a user row                                             |
| `check-user-pwd`   | Verify a password against credentials                         |
| `set-user-pwd`     | Set a user's password                                         |
| `roles`            | List roles                                                    |
| `user-roles`       | List a user's roles                                           |
| `user-authzs`      | Effective authzs of a user (after role inheritance)           |
| `set-max-sessions` | Bound concurrent sessions for a user                          |

All are declared with `SDF_AUTHZ_X`, intending to require
`__execute_command__` — but see §4.5: that flag is currently unread.

Agent-side: `cmd_authzs_yuno` (`c_agent.c:5957-6003`, registered as
`authzs-yuno` at `c_agent.c:898`) is the agent's wrapper to broadcast
authz data to all running yunos.

---

## 6. TLS

`ytls` (`kernel/c/ytls/`) is the runtime-selectable OpenSSL / mbedTLS
abstraction. Every TCP gclass gets a `ytls` pointer and a `use_ssl`
boolean. See [`IPC.md`](IPC.md) §6.6 for how TLS is hooked into the
gate stack.

The interesting operational machinery in production is **certificate
auto-sync**: keeping cert files fresh as letsencrypt rotates them.

### 6.1 cert_sync — overview

Driven by the agent. Periodically runs a "copy certs" command, diffs
the result, and broadcasts a reload event to every yuno if anything
changed. Yunos that hold TLS listeners reload from disk without
dropping live connections.

```
        agent's cert_sync_timer (default 900 s)
                │  every interval:
                ▼
        snapshot /yuneta/store/certs                ← before
                │
        run cert_sync_copy_cmd (sudo)                ← e.g. copy from
                │                                     /etc/letsencrypt
                ▼
        snapshot /yuneta/store/certs                ← after
                │
        diff before vs after
                │
        ┌───────┴────────┐
        │                │
   no change         changed
        │                │
        │                └─► publish reload-certs to every running yuno
        │                    │
        │                    ▼
        │                yuno's C_TCP_S re-reads its cert from disk
        │                without closing existing connections
        ▼
   last_check ← now
```

### 6.2 The agent's `cert_sync_*` attributes

`c_agent.c:937-944`:

| Attribute               | Default                                          | Purpose                                  |
|-------------------------|--------------------------------------------------|------------------------------------------|
| `cert_sync_enabled`     | `1`                                              | Master enable                            |
| `cert_sync_interval_sec`| `900` (15 min)                                   | How often to check                       |
| `cert_sync_store_dir`   | `/yuneta/store/certs`                            | Directory the yunos read certs from      |
| `cert_sync_copy_cmd`    | `/usr/bin/sudo -n /yuneta/store/certs/copy-certs.sh` | Command run on every tick           |
| `cert_sync_last_check`  | `0`                                              | Unix ts, updated on tick                 |
| `cert_sync_last_action` | `0`                                              | Unix ts, updated when a change applies   |
| `cert_sync_last_result` | `""`                                             | `ok` / `skipped` / `error`               |
| `cert_sync_failures`    | `0`                                              | Cumulative failure counter               |

### 6.3 The `copy-certs.sh` convention

The default `cert_sync_copy_cmd` shells out via `sudo -n` to a script you
control:

```
/usr/bin/sudo -n /yuneta/store/certs/copy-certs.sh
```

Typical content (deployer-supplied, not shipped by yunetas):

```bash
#!/bin/bash
# /yuneta/store/certs/copy-certs.sh
set -e
cp /etc/letsencrypt/live/example.com/fullchain.pem /yuneta/store/certs/example.com.crt
cp /etc/letsencrypt/live/example.com/privkey.pem   /yuneta/store/certs/private/example.com.key
chown yuneta:yuneta /yuneta/store/certs/*.crt /yuneta/store/certs/private/*
```

The `sudo -n` requires NOPASSWD in sudoers — a wide grant; see §8.10.

### 6.4 The reload broadcast

`c_agent.c:8926-8942`: when the post-snapshot diff says "changed",
`cert_sync_broadcast_reload()` sends `command=reload-certs service=__yuno__`
to every running yuno via `cmd_command_yuno()`, plus the local agent.

Yunos without TLS listeners ignore the event. Yunos with TLS handle it
at `c_tcp_s.c:854-885` — re-read the cert paths configured in their
`crypto` attribute, swap the new cert into the listening context, leave
existing connections alone.

### 6.5 `cert-sync-now` and `cert-sync-status`

`cmd_cert_sync_now` (`c_agent.c:6694-6711`) forces a tick immediately.
`cmd_cert_sync_status` (`c_agent.c:6717-6750`) returns the full state:
`enabled`, `interval_sec`, `store_dir`, `copy_cmd`, `last_check`,
`last_action`, `last_result`, `failures`, plus a
`deploy_hook_last_run` timestamp read from
`/var/lib/yuneta/last-deploy-hook-run` if present.

### 6.6 How a yuno reads its cert paths

Direct from disk via its config. Example from
`batches/localhost/auth_bff.1801.json:26-27`:

```json
"crypto": {
    "ssl_certificate":     "/yuneta/store/certs/localhost.crt",
    "ssl_certificate_key": "/yuneta/store/certs/private/localhost.key"
}
```

The yuno does **not** know about cert-sync. It just re-reads these
paths when `reload-certs` arrives. Cert-sync is the producer; the
yuno's `crypto` block is the consumer; they communicate only via the
filesystem and the reload event.

---

## 7. Per-project Keycloak realms

The convention from
[`auth_bff/README.md`](../auth_bff/README.md#deployment):
**one auth_bff instance per Keycloak realm**, **one realm per project**.

### 7.1 Project-realm mapping (known production state)

| Project           | Keycloak host                  | Realm name           | Notes                                              |
|-------------------|--------------------------------|----------------------|----------------------------------------------------|
| yunetas dev       | `auth.artgins.com`             | `yunetas.com`        | Localhost dev batch, see `batches/localhost/auth_bff.1801.json`. |
| wattyzer          | (per project, private repo)    | (per project)        | See wattyzer batches/.                              |
| estadodelaire     | (per project, private repo)    | (per project)        | See estadodelaire batches/.                         |

### 7.2 Bootstrap checklist for a new project

1. Create the realm in Keycloak (`<project>` or `<project>connect`).
2. Register the OAuth2 client in that realm:
   - Public client (no client_secret) if browser-only.
   - Confidential client (with secret) if server-to-server.
   - Set `Valid Redirect URIs` to the BFF's callback.
   - Set `Web Origins` to the SPA's origin.
3. Write `yunos/c/auth_bff/batches/<host>/auth_bff.<port>.json`:

   ```json
   {
       "issuer":               "https://auth.<project>.example/realms/<realm>/",
       "client_id":            "<client_id>",
       "client_secret":        "",
       "cookie_domain":        "<project>.example",
       "allowed_redirect_uri": "https://<project>.example/auth/callback"
   }
   ```

4. Provision a TLS cert for `<project>.example` + `auth.<project>.example`
   under `/yuneta/store/certs/` (or however the project's `cert_sync_copy_cmd`
   delivers it).
5. `install-binary` + `create-config` + `create-yuno` for the auth_bff
   (see [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md) §6.1, [`SCAFFOLDING.md`](SCAFFOLDING.md)
   §10.1).

---

## 8. Sharp edges

### 8.1 `client_secret` in cleartext in batches

The localhost batch shows `client_secret: ""` (empty), but production
batches in private repos commit the real secret in cleartext JSON. There
is no encrypted-secret-store integration today. If you commit a
production batch to git, the secret is in history forever — rotate it
in Keycloak first.

### 8.2 SMTP password in cleartext

`stress/c/listen/deploy-yuno/emailsender.artgins.json:7` has an SMTP
password field in cleartext (the public repo example carries a
placeholder, but the private repos have the real value). See
`project_emailsender_smtp_secret`:
pending env-var migration + rotation as of 2026-05-15. The same secret
also lives in the agent's treedb at runtime.

### 8.3 The command authz check is commented out

`command_parser.c:73-113`. The most important thing in this document.
`gobj_user_has_authz` is **not invoked** for commands. `SDF_AUTHZ_X` is
silently ignored. Until that block is uncommented, every authenticated
user can run every command. Plan accordingly:

- Don't rely on `pm_*` schemas for security today.
- For commands that **must** be gated right now, call
  `gobj_user_has_authz` explicitly at the top of the handler.
- Be aware that uncommenting it is a breaking change for any deployment
  that hasn't already assigned roles.

### 8.4 Event-level authz is also unenforced

`EVF_AUTHZ_INJECT` and `EVF_AUTHZ_SUBSCRIBE` (`gobj.h:332-333`) are
declared and the global authzs `__inject_event__` /
`__subscribe_event__` are registered (`gobj.c:418-419`), but no check
runs in `gobj_send_event` or `gobj_subscribe_event`. Same status as
commands: declared, not enforced.

### 8.5 Authz default is allow

`gobj_user_has_authz` returns `TRUE` if no checker is installed
(`gobj.c:9438-9448`). A yuno that did not register `C_AUTHZ` has zero
authz enforcement, even for the custom `gobj_user_has_authz` calls
inside individual gclasses. The default is open.

### 8.6 The `yuneta` bypass is authentication-only

`c_authz.c:796-815` permits the `yuneta` user to authenticate without
JWT/password. It does **not** give `yuneta` automatic authz over
everything; the user still has to own roles. In practice the agent's
`yuneta` user owns every role in production, but a fresh deployment
can authenticate as `yuneta` and still hit "no permission" on a
custom-gated operation.

### 8.7 Legacy `idp_url` + `realm` still works

The deprecation warning is logged but the BFF accepts the legacy shape
and constructs the URL automatically (`c_auth_bff.c:358`). Don't rely
on this — migrate the batches.

### 8.8 HTTP_CL chain leak on rapid disconnect

`c_auth_bff.c:387-413, 1910-1978`. During load testing with aggressive
client disconnects mid-`/token`, the outbound HTTP client chain isn't
always cleaned up. Watch the process's open-fd count when load is
unusual.

### 8.9 No real-IdP smoke tests

`tests/c/c_auth_bff/` runs against `c_mock_keycloak.c`. Regressions
against a real Keycloak release go unnoticed in CI. Manual smoke test
on staging is mandatory before any auth_bff release.

### 8.10 `cert_sync_copy_cmd` requires NOPASSWD sudo

`sudo -n /yuneta/store/certs/copy-certs.sh`. Pick the **smallest
possible NOPASSWD scope** — ideally only that exact script path. A
careless `yuneta ALL=(ALL) NOPASSWD: ALL` line in sudoers turns the
yuno process into a full-root foothold. Cert-sync needs nothing more
than the one script.

### 8.11 Cert-sync is host-global

The `cert_sync_*` attrs are on the agent, not on the realm. One host
shares one cert directory and one copy command across every realm.
If two realms need disjoint certs you cannot achieve it through
cert-sync — partition by host or ship cert paths directly via per-yuno
config.

### 8.12 Cookie Domain is shared across all yunos on the host

The BFF sets `Domain=<host>` with no port. A cookie set by the BFF on
:1801 is automatically sent to the WebSocket on :1800, :1600, etc. on
the same hostname. This is by design (lets the SPA hop between
services) but it means any yuno on the same hostname can read the
cookie if it chooses to. Don't run an untrusted yuno on the same
hostname as the BFF.

### 8.13 `reload-certs` is broadcast unconditionally

Every running yuno receives the event. Yunos that don't use TLS just
no-op the handler. If a yuno's `reload-certs` handler has a bug, the
cert change cascades into a noisy error in every log — but the cert
itself does propagate. The broadcast is best-effort, not transactional.

---

## 9. Recipes

### 9.1 Set up auth_bff for a new project (with Keycloak)

```bash
# 1. realm + client in Keycloak first
#    - realm name: <project>connect (convention)
#    - client: public + PKCE, valid redirect uri = https://<project>.example/auth/callback

# 2. write the batch config
cat > /yuneta/development/yunetas/yunos/c/auth_bff/batches/<host>/auth_bff.1801.json <<'EOF'
{
    "issuer":               "https://auth.<project>.example/realms/<project>connect/",
    "client_id":            "<project>-spa",
    "client_secret":        "",
    "cookie_domain":        "<project>.example",
    "allowed_redirect_uri": "https://<project>.example/auth/callback"
}
EOF

# 3. cert in /yuneta/store/certs/ (provisioned by your copy-certs.sh)

# 4. install + create + run via the agent (see YUNO_LIFECYCLE.md §6.1)
```

### 9.2 Migrate a legacy `idp_url` + `realm` batch to `issuer`

```diff
- "idp_url":  "https://auth.example.com",
- "realm":    "yunetas.com",
+ "issuer":   "https://auth.example.com/realms/yunetas.com/",
```

That's it — the deprecation warning will stop firing on next start.
Verify the issuer URL with curl against
`<issuer>.well-known/openid-configuration`.

### 9.3 Add a user via `C_AUTHZ` commands

```bash
# create
ycommand -c 'command-yuno id=<yuno> service=authz command=create-user id=alice'

# assign roles (the user must already have an empty roles[] field)
ycommand -c 'command-yuno id=<yuno> service=authz command=add-user-role user_id=alice role_id=operator'

# set password (if using ROPC)
ycommand -c 'command-yuno id=<yuno> service=authz command=set-user-pwd user_id=alice pwd=<...>'

# inspect
ycommand -c 'command-yuno id=<yuno> service=authz command=user-authzs user_id=alice'
```

### 9.4 Add a role with limited authzs

```bash
ycommand -c 'command-yuno id=<yuno> service=authz command=create-role id=read_only service=__yuno__ permission=__read_attribute__'
ycommand -c 'command-yuno id=<yuno> service=authz command=user-roles user_id=alice'
```

Remember §8.3 — until the command authz check is uncommented in
`command_parser.c`, role assignments don't actually restrict command
execution.

### 9.5 Rotate TLS certs

Typical letsencrypt flow:

```bash
# 1. let certbot renew (cron / systemd timer on the host)
sudo certbot renew --quiet

# 2. cert_sync runs on the agent's next tick (default 15 min);
#    to force it sooner:
ycommand -c 'cert-sync-now'

# 3. inspect
ycommand -c 'cert-sync-status'
# expect:
#   last_action: <recent timestamp>
#   last_result: ok
#   failures:    0

# 4. confirm yunos are using the new cert
openssl s_client -connect <host>:<port> -showcerts </dev/null 2>/dev/null \
    | openssl x509 -noout -dates
```

### 9.6 Diagnose "no permission" failures

Today (`SDF_AUTHZ_X` unread) "no permission" only fires from explicit
`gobj_user_has_authz` calls inside specific gclasses (e.g. `C_AUTHZ`'s
own self-management commands).

```bash
# 1. who am I, according to the yuno?
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=view-attrs name=__username__'

# 2. what does the authz service say my authzs are?
ycommand -c 'command-yuno id=<yuno> service=authz command=user-authzs user_id=<me>'

# 3. enable the authzs trace globally to see the predicate's verdict
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-global-trace level=authzs set=1'
tail -F /yuneta/logs/<yuno>/*.log | grep -a '"msg":' | grep -i authz
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-global-trace level=authzs set=0'
```

If the trace shows the predicate returning `TRUE` but the operation
still rejects, the rejection is from a different gate (cookie domain
mismatch, JWT expiry, account `disabled=true`). Look at the BFF and
`C_AUTHZ` logs.

---

## 10. Code pointers

| What                                              | Where                                                                  |
|---------------------------------------------------|------------------------------------------------------------------------|
| `C_AUTH_BFF` gclass                               | `kernel/c/root-linux/src/c_auth_bff.c`                                 |
| auth_bff yuno wrapper                             | `yunos/c/auth_bff/src/c_auth_bff_yuno.c`                               |
| auth_bff endpoints dispatcher                     | `c_auth_bff.c:2110-2236`                                               |
| auth_bff attrs (`issuer`, deprecated `idp_url`)   | `c_auth_bff.c:181-192`                                                 |
| PKCE token call                                   | `c_auth_bff.c:1383-1476`                                               |
| Cookie builder                                    | `c_auth_bff.c:748-770`                                                 |
| libjwt entry point                                | `kernel/c/libjwt/src/jwt-verify.c:83`                                  |
| `C_AUTHZ` gclass                                  | `kernel/c/root-linux/src/c_authz.c`                                    |
| `authzs` treedb schema                            | `kernel/c/root-linux/src/treedb_schema_authzs.c:58-343`                |
| Role inheritance walk                             | `c_authz.c:3254-3266` (`get_user_roles`)                               |
| `yuneta` super-user bypass                        | `c_authz.c:796-815`                                                    |
| `__username__` write-side                         | `c_authz.c:945, 969, 1177`                                             |
| `gobj_user_has_authz`                             | `gobj.h:1607-1611`, `gobj.c:9400-9452`                                 |
| `SDATAPM` / `SDATAAUTHZ` macros                   | `gobj.h:218-263`                                                       |
| Commented-out command authz check                 | `kernel/c/gobj-c/src/command_parser.c:73-113`                          |
| `EVF_AUTHZ_*` flags                               | `gobj.h:332-333`                                                       |
| Agent's cert_sync attrs                           | `yunos/c/yuno_agent/src/c_agent.c:937-944`                             |
| `cert_sync_tick` (diff + broadcast)               | `c_agent.c:8944-8989`                                                  |
| `cert_sync_broadcast_reload`                      | `c_agent.c:8926-8942`                                                  |
| `cert-sync-now` / `cert-sync-status` commands     | `c_agent.c:6694-6750`                                                  |
| `reload-certs` handler in TCP server              | `kernel/c/root-linux/src/c_tcp_s.c:854-885`                            |
| Per-yuno cert paths (example)                     | `yunos/c/auth_bff/batches/localhost/auth_bff.1801.json:26-27`          |
| Localhost dev OIDC batch                          | `batches/localhost/auth_bff.1801.json:55-58`                           |
| auth_bff pending bugs (memory)                    | `~/.claude/.../memory/project_auth_bff_pending_bugs.md`                |
| SMTP cleartext (memory)                           | `~/.claude/.../memory/project_emailsender_smtp_secret.md`              |
