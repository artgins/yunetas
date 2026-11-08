# Auth GClasses

Authentication, authorization, and OAuth 2.

**Source:** `kernel/c/root-linux/src/c_authz.c`, `c_auth_bff.c`,
`c_idp_keycloak.c`, `c_task_authenticate.c`

---

(gclass-c-authz)=
## C_AUTHZ

Authorization manager — maintains a JSON Web Key Set (JWKS), verifies
JWT tokens, and manages users and their access rules.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |

### Commands

| Command | Description |
|---------|-------------|
| `list-jwk` / `add-jwk` / `remove-jwk` | Manage JSON Web Keys: list the running set, or change it until the yuno restarts. The persistent set is the config's (`Authz.jwks`). |
| `users` / `create-user` / `update-user` / `enable-user` / `disable-user` / `delete-user` | User management. A `role` given to `create-user` / `update-user` is a ref `roles^<role id>^users` to a role that exists; anything else is refused before the user is written (*"Role does not exist"*, *"Bad role ref, expected roles^ROLE^users"*), and the user keeps the roles it had. Example: `ycommand -c 'command-yuno id=<id> service=authz command=update-user username=ana@example.com role=roles^operator^users'`. On a replica every write is refused (*"READ-ONLY replica"*), the commands AND the events: `EV_ADD_USER` and `EV_IDP_USER_CREATED` return `-1` and log *"READ-ONLY replica, the users store cannot be written here"* (after 7.25.3). |
| `enable-user` / `disable-user` / `set-max-sessions` | Write ONE column of the user -- `disabled`, or `max_sessions` -- and nothing else; `disable-user` also drops the user's live sessions, and `set-max-sessions` without `username` sets the service default `max_sessions_per_user`. Until 7.25.4 they wrote back the whole view they had read, whose hidden `credentials` is a `null` mask, and so ERASED the user's local password. Example: `ycommand -c 'command-yuno id=<id> service=authz command=set-max-sessions username=ana@example.com max_sessions=3'`. |
| `accesses` | List access rules. |

**Every command from a peer asks a permission of the users treedb**
(`treedb_authzs`), whatever `enable_command_authz` says: `read` to list,
`create` / `update` / `delete` to manage users (`create-user` with a `role`
asks `update` too, as `link-nodes` does), `update` for the JWKs,
`set-max-sessions` and `check-user-pwd`. Without it the answer is `-403`. Up
to 7.25.21 they were `SDF_AUTHZ_X` only, which does nothing while the gate is
off: any user the entry gate let in managed users and the trusted signing keys.
`tests/c/command_delete_user`, case 12.

**Every comment starts with the yuno that answers** (new after 7.25.4; in
7.25.4 "User enabled: x", "Set max_sessions: ..." and "User not found" did not):
`command-yuno id=<id> service=authz command=enable-user username=pepe@example.com`
answers `0: <role^name>: User enabled: pepe@example.com`, and an unknown user
`-1: <role^name>: User not found: 'nobody@example.com'`. A failure says its
own cause and points at the log, e.g. `<role^name>: cannot hash the password of
user 'pepe@example.com' (see the log)`: `create-user`, `update-user` and
`set-user-pwd` answered `gobj_log_last_message()`, the process-global buffer of
the last ERROR of anybody. The test walks C_AUTHZ's command table
(`tests/c/command_delete_user`, case 11).

**`master` is configuration, default `false`, never set by the service.** With
an empty `tranger_path` the path is built from `authz_service` (or the yuno
role), the realm and `authz_tenant`; only a master creates the directory. A
yuno that owns its users says so -- the agent, in its `main.c`:

```
'global': {
    'Authz.master': true,
    'Authz.authz_service': 'agent'
}
```

Without it, a store that does not exist gives *"No authz db, authz only to
local access"* and no treedb, and a store another yuno owns is opened as a
READ-ONLY replica.

`with_link_events` (`SDF_RD`, default `1`, new in 7.26.0) is copied to the
`C_NODE` of `treedb_authzs`: a link of a user to a role publishes
`EV_TREEDB_NODE_LINKED`, not the role's `EV_TREEDB_NODE_UPDATED`. A yuno
whose **v1** SPA edits `treedb_authzs` turns it off in its config:

```json
{"name": "authz", "gclass": "C_AUTHZ", "kw": {"with_link_events": false}}
```

(authz-trust-peer-ip)=
### A login by the peer's address, and a proxy (`trust_peer_ip`)

A peer with no token and no password logs in by its **address** alone:

- from `127.0.0.*`, as the local user `yuneta`, with its real roles. The
  seed of every yuno gives `yuneta` the `root` role, so this is a superuser
  that reaches every service of the yuno, `__yuno__` included. This is how
  `ycommand` logs in to the agent of its own node;
- from an address of the yuno's `allowed_ips` (`add-allowed-ip`), as the
  user it names (`yuneta`, unless `allow_anonymous_in_localhost`).

**The trap: a proxy on the same host.** nginx (or any proxy) on the host of
a yuno, in front of its `C_IEVENT_SRV`, connects from `127.0.0.1`. Every
browser behind it then reaches the yuno from `127.0.0.1`, and one that sends
an identity card with no token and the user `yuneta` is `yuneta`, root, with
no login at all. A firewall does not help (the peer IS the local host), nor
does a denied address (it cuts the users with a token too), nor does deleting
`yuneta` (a seed record cannot be deleted).

A yuno behind such a proxy sets `trust_peer_ip` to `false`:

| Attribute | Default | Meaning |
|-----------|---------|---------|
| `trust_peer_ip` | `true` | A peer with no token and no password logs in by its address (`127.0.0.*`, `allowed_ips`). `false`: never; a token or a password is the only way in. Set in the config only (`SDF_RD`). |

```json
"global": {
    "Authz.trust_peer_ip": false
}
```

With it `false`, such a card is refused with a WARNING *"Without JWT/passw
the peer address is not trusted"* (`username`, `peername`), and the client
gets that comment. A token (the BFF cookie, or the `jwt` of the card) and a
password log in as before. The default stays `true`: the agents need it, and
so does any yuno that the CLI of its node reaches without a token. The
setting is per yuno, not per listener: all the logins of a yuno go through
its one `C_AUTHZ`. So a yuno behind a proxy whose local clients log in
without a token must give them one (or a password) before it turns the
setting off. Until then, a stopgap only: make the proxy connect from an
address out of `127.0.0.*`, nginx `proxy_bind 127.0.1.1;` in each `location`
it proxies to the yuno -- and keep that address out of `allowed_ips`.

`tests/c/authz_login` (`_no_peer_ip` with it `false`).

**What a refused login writes.** A token that does not verify gives the
WARNING *"Authentication failed: Invalid token"* with its `status`, and the
token only as `jwt_sha256` (the first 16 hex characters of its sha256) and
`jwt_length`: never the token itself. Up to 7.30.14 the whole token was in
the line, so a token good elsewhere that reached the wrong port ended in the
log. A token with no `email` or no `email_verified` is refused with a
WARNING (*"Username must be an email address"*, *"Email not verified"*);
up to 7.30.14 the reader logged an ERROR first. The claims of a token whose
`exp`, `nbf`, `iss`, `sub` or `aud` fails are not traced either: libjwt
traced them all, unverified, before the signature was checked; it traces
which claims failed (`failed_claims=0x...`) now.

---


### The age of a login token (`max_token_age`, 7.30.14+)

The identity provider decides how long its tokens live. A Keycloak access
token can live 20 days. `max_token_age` is Yuneta's own limit, whatever the
identity provider gives: a login token opens a session only if it was issued
(its `iat`) no more than `max_token_age` seconds ago.

| Attribute | Default | Meaning |
|-----------|---------|---------|
| `max_token_age` | `0` | Seconds a login token may have lived (`now - iat`) to open a session. `0`: no limit. With a limit, a token with no `iat` is refused. |
| `token_age_exempt_users` | `[]` | Users (the `email` of the token) whose token is not checked. Use it only for a token that cannot be renewed. |

Example: one hour, and the shared token of the agents of before 7.30.10,
which cannot be renewed (see the
[controlcenter](../../yunos/controlcenter.md)):

```json
"global": {
    "Authz.max_token_age": 3600,
    "Authz.token_age_exempt_users": ["agent@example.com"]
}
```

What the limit does:

- Only the login is checked. A session that is open stays open when its
  token gets older.
- A refusal is a WARNING *"Authentication failed: token too old"*, with
  `token_age`, `max_token_age` and the user, never the token. The client gets
  the comment *"The token is too old"* or *"The token has no iat"*.
- The local method `verify_jwt` without an `audience` (a login token) refuses
  the same tokens, without a log.
- The age is this yuno's clock against the identity provider's `iat`. Give
  the limit some minutes more than the time a client needs.

A client must get a new token before it logs in again. `ycommand` and the
other CLI tools log in with a new token on each run. A SPA behind
[`C_AUTH_BFF`](#gclass-c-auth-bff) that reconnects with an old cookie gets a
refused identity card; the agent console then refreshes its token
(`/auth/refresh`) and connects again.

### Operator assertions (`assertion_jwks`, 7.30.9+)

An **operator assertion** is a short token that a control center signs for
**one node**. It says which operator asks a command that the control center
forwards to that node's agent. `C_AUTHZ` keeps the public keys of its signers
in a set of their own, `assertion_jwks`. Only the local method `verify_jwt`
reads this set, and only when it is asked with an `audience`. A login never
reads it, so an assertion cannot open a session.

| Attribute | Default | Meaning |
|-----------|---------|---------|
| `assertion_jwks` | `[]` | Public keys (JWK) of the signers. The `kid` is the issuer (`iss`) of their assertions. |
| `max_assertion_lifetime` | `300` | Seconds an assertion may live (`exp - iat`). A longer one is refused, as is one with no `iat` or `exp`. |
| `assertion_time_leeway` | `0` | Seconds this yuno's clock may be off the signer's when `exp` and `nbf` are checked. |

Example: the agents' `main.c` holds the control center's public key (EC
P-256, ES256) and one day of leeway. A box with no NTP must still take its
control center's commands. What binds an assertion is its audience, not its
time.

```json
"global": {
    "Authz.assertion_time_leeway": 86400,
    "Authz.assertion_jwks": [
        {
            "kid": "controlcenter",
            "kty": "EC",
            "crv": "P-256",
            "alg": "ES256",
            "use": "sig",
            "x": "pfcUj6lfWF5NlMBi47QQt0NPtGs570TPZews4VJLtlE",
            "y": "e5jGmxU2VE8If70nC1AbP2rVL18R6Y3tFSDSEb7-E0Q"
        }
    ]
}
```

The local method `verify_jwt` takes `{"jwt": "<token>", "audience": "<node
uuid>"}` and answers `{"result": 0, "username": "<sub>"}`, or `{"result": -1,
"comment": "<why>"}`:

```c
json_t *jn_resp = gobj_local_method(gobj_authz, "verify_jwt",
    json_pack("{s:s, s:s}", "jwt", token, "audience", node_uuid()),
    gobj
);
```

A good assertion has these properties:

- a key of `assertion_jwks` signed it;
- it is valid now, give or take `assertion_time_leeway`;
- its `aud` is the audience;
- its `iat` and `exp` are no more than `max_assertion_lifetime` apart;
- its `sub` names the operator. The `sub` can be an email or a local user of
  the signer, such as `yuneta`.

A user of this yuno that is disabled is refused. Without an `audience`,
`verify_jwt` checks a login token instead: a token of an issuer of `jwks`,
for a verified email, not older than `max_token_age`. The client side is
[`C_IEVENT_CLI`'s `verify_operator`](#gclass-c-ievent-cli). The signer is the
[controlcenter](../../yunos/controlcenter.md).

(gclass-c-auth-bff)=
## C_AUTH_BFF

Backend-For-Frontend OAuth 2 server — mediates between browser SPAs and the
IdP (OIDC; Keycloak in production), storing the tokens in httpOnly cookies.
The guide is [OAuth2 + PKCE with a BFF](../../guide/guide_oauth2_pkce_bff.md).

| Property | Value |
|----------|-------|
| **States** | `ST_IDLE` (one state: each request goes through a `C_TASK` to the IdP) |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `issuer` | `string` | OIDC issuer URL; the endpoints come from its `/.well-known/openid-configuration`. |
| `token_endpoint` | `string` | Explicit token endpoint; with `end_session_endpoint`, skips discovery. |
| `end_session_endpoint` | `string` | Explicit end-session (logout) endpoint. |
| `client_id` | `string` | OAuth 2 client ID. |
| `client_secret` | `string` | OAuth 2 client secret (empty for a public client). |
| `cookie_domain` | `string` | `Domain` of the cookies, written only when the request `Host` is that domain or a subdomain. |
| `allowed_origin` | `string` | The SPA's origin: the CORS origin and the only `Origin` served for login, callback, refresh, logout and token (else `403 origin_not_allowed`). It is what pins the scheme. Empty: the `Origin` must be the BFF's own (`https://` and the `Host`, or `http://` and it on a connection that is not TLS), and `/auth/token` is refused. Example: `"allowed_origin": "https://app.example.com"`. |
| `allowed_redirect_uri` | `string` | The `redirect_uri` a `/auth/callback` may have: exactly it, or, ending with `/` after a scheme and a host, a path under it (no `.`, `..` or empty segment but the last, no `\`, no `#`). Example: `"allowed_redirect_uri": "https://app.example.com/app/"`. |
| `expose_access_token` | `boolean` | Opt-in `POST /auth/token` (default `false`). |
| `crypto` | `dict` | TLS configuration of the IdP calls (verifying by default). |
| `pending_queue_size` | `integer` | Requests queued per channel (default 16). |
| `idp_timeout_ms` | `integer` | Round-trip limit of an IdP call (default 30000; 0 or less is that default, a round-trip is always bounded). |
| `idp_connect_timeout_ms` | `integer` | Connect limit of an IdP call (default 30000). |
| `idp_logout_timeout_ms` | `integer` | Limit of a whole logout, the wait for the discovery and the connection and the resend included (default 10000; 0: only the two above). A logout answers within it, cookies cleared, so the answer reaches a SPA that waits 15 s. |

### Endpoints

| Endpoint | Description |
|----------|-------------|
| `POST /auth/login` | Password grant; writes the cookies. |
| `POST /auth/callback` | PKCE code exchange; writes the cookies. |
| `POST /auth/refresh` | Refresh with the `refresh_token` cookie; answers the identity too. |
| `POST /auth/logout` | Ends the session at the IdP and clears the cookies. With no `refresh_token` cookie, no IdP call: it only clears them, for a request with the `access_token` cookie; with no cookie, `200` and no `Set-Cookie`. A logout served (a cookie, and the SPA's `Origin`) clears the cookies with every answer, its `400`, `500`, `502`, `503` and `504` too, within `idp_logout_timeout_ms`; a cut connection sends the end-session once more. |
| `POST /auth/token` | Opt-in (`expose_access_token`): the access token for another backend host. |

Every endpoint but `OPTIONS` that reads or changes the session (login, callback,
refresh, a logout with a cookie, token) is served only to the `Origin` of the
SPA (see `allowed_origin`), else `403 origin_not_allowed` with no IdP call.

A token lifetime of 0 from the IdP is a token with no expiry (Keycloak's
offline token, `"refresh_expires_in": 0`); its cookie has no `Max-Age`:

```
Set-Cookie: refresh_token=<jwt>; Path=/; HttpOnly; Secure; SameSite=Strict
```

---

(gclass-c-idp-keycloak)=
## C_IDP_KEYCLOAK

Manages the ACCOUNTS of a Keycloak identity provider through its admin REST
API: create, list, read, change, delete, and email the user the link to set
the password. It is not part of `C_AUTHZ`, which answers one question (does
this user hold this permission); managing an external provider is another
responsibility, with other credentials, another transport and other failure
modes. Instantiated as the service **`idp`**, with neutral command names
(`list-idp-users`, not `list-kc-users`): a second provider would be a
sibling gclass serving the same commands, chosen in configuration.

**The seam is an event.** When it creates an account it publishes
`EV_IDP_USER_CREATED` (`username`, `first_name`, `last_name`, `role`,
`idp_user_id`), and each plane provisions itself -- `C_AUTHZ` writes the
authorization node. `delete-idp-user` does NOT touch the local authz record.

**One request at a time.** Every command enters one queue and one volatile
`C_TASK` drives it through the shared HTTP client; the admin token is job 0
and is skipped while the cached one is fresh. The queue holds 32: a caller
beyond that is refused, never left waiting.

**What goes again.** `C_TASK` runs its job again each time the connection
opens. What only reads (the admin token, a list, a read) goes up to 3 times
when the connection is cut before its answer: a restart of the IdP costs
nothing. What changes something (create, update, delete, actions, the
invitation email) goes once, and a cut answers `kc_unavailable` (the invitation:
registered, with the warning `email_send_failed`). An IdP that always cuts
ends in `kc_unavailable` too, with a WARNING, not in a queue held for ever. A
token longer than the 8191 bytes the gclass keeps is `kc_unavailable` with a
WARNING, not a token cut. Tests: `c_task_authenticate/test18_idp_keycloak`.

| Property | Value |
|----------|-------|
| **Service** | `idp` |
| **Output event** | `EV_IDP_USER_CREATED` (optional subscribers) |
| **Trace levels** | `messages` -- the requests and answers of the admin API |

### Attributes

The connection is configured at run time with `set-kc-config`, which saves
it (persistent attributes): the values are per deployment and the secret
never goes into code or committed configuration.

| Attribute | Type | Description |
|-----------|------|-------------|
| `kc_base_url` | `string` | Keycloak base URL (persistent). |
| `kc_realm` | `string` | Realm where the accounts live (persistent). |
| `kc_admin_client_id` | `string` | Confidential admin client, `client_credentials`, role `manage-users` (persistent). |
| `kc_admin_client_secret` | `string` | Its secret (persistent; `view-kc-config` masks it). |
| `kc_redirect_uri` | `string` | `redirect_uri` of the set-password email (persistent). |
| `kc_email_client_id` | `string` | Client the email links to: the SPA's (persistent). |
| `kc_crypto` | `json` | TLS of the calls to Keycloak. Verifies by default against the system CA; a private CA is `{"ssl_trusted_certificate": "/path/ca.pem"}`. mbedTLS has no system store: set `ssl_trusted_certificate` there. |
| `kc_timeout_ms` | `integer` | Watchdog of one round trip, default `30000`. |

### Commands

| Command | Permission | Description |
|---------|------------|-------------|
| `set-kc-config` | `configure-kc` | Set and save the connection; only the parameters passed change. |
| `view-kc-config` | `configure-kc` | Show it, secret masked. |
| `register-idp-user` | `register-idp-user` | Create an account (`email` required, used as username; `firstName`, `lastName`). The user gets the email to set the password. Publishes `EV_IDP_USER_CREATED`. `role` is legacy: roles belong to the authorization plane, which applies its `default_role`. A `role` given by a peer needs, besides `register-idp-user`, the `update` of `treedb_authzs` that `create-user` asks to link one (else `-403`). |
| `list-idp-users` | `manage-idp-users` | List the accounts: `search` (username, names, email), paging `first` / `max` (default 50, ceiling 500), `brief=0` for the full representation. |
| `get-idp-user` | `manage-idp-users` | One account, by `user_id` (the uuid Keycloak gives, not the email). |
| `update-idp-user` | `manage-idp-users` | Change `firstName`, `lastName`, `enabled`, `emailVerified`, `requiredActions`; an absent parameter is left unchanged. |
| `delete-idp-user` | `manage-idp-users` | Delete the account in the IdP. The local authz record is not touched. |
| `send-idp-user-actions` | `manage-idp-users` | Email the user the link to run `actions` (default `["UPDATE_PASSWORD"]`; e.g. `VERIFY_EMAIL`). |

`user_id` also answers to `id`, which is unusable through `command-yuno`
(the agent reads `id` as the yuno to address).

`brief`, `enabled`, `emailVerified`, `requiredActions` and `actions` are
`DTP_JSON` (since SDK 7.27.0; they were `DTP_STRING`, which the command
parser now holds to a string): a boolean is taken as `true`/`false`, `1`/`0`
or its text, a list as a json list, its text, or one bare word.

```bash
ycommand -c 'command-yuno id=<yuno> service=idp command=update-idp-user user_id=<uuid> enabled=0 requiredActions=VERIFY_EMAIL'
ycommand -c 'command-yuno id=<yuno> service=idp command=send-idp-user-actions user_id=<uuid> actions=VERIFY_EMAIL'
```

```js
// from a SPA: the json shapes
gobj_command(gobj_remote_idp, "update-idp-user",
    {user_id: uuid, enabled: false, requiredActions: []}, gobj);
```

**Example**

The service in a yuno's `main.c`, and its first configuration:

```c
{                                                               \n\
    'name': 'idp',                                              \n\
    'gclass': 'C_IDP_KEYCLOAK',                                 \n\
    'autostart': true                                           \n\
},                                                              \n\
```

```bash
ycommand -c "command-yuno id=7301 service=idp command=set-kc-config \
    kc_base_url=https://auth.example.com kc_realm=example \
    kc_admin_client_id=yuneta-admin kc_admin_client_secret=<secret> \
    kc_redirect_uri=https://app.example.com/ kc_email_client_id=app"
ycommand -c "command-yuno id=7301 service=idp command=register-idp-user email=ana@example.com firstName=Ana"
ycommand -c "command-yuno id=7301 service=idp command=list-idp-users search=ana"
```

Details of the provisioning flow: [`YUNO_AUTH.md`](../../../../yunos/c/yuno_agent/YUNO_AUTH.md).

---

(gclass-c-task-authenticate)=
## C_TASK_AUTHENTICATE

OAuth 2 login of a program with no browser (`ycommand`, `ycli`, `ystats`, ...):
a `C_TASK` that runs the OIDC discovery (when only `issuer` is given), a password
grant and an end-session over its own `C_PROT_HTTP_CL`, and publishes the token
in `EV_ON_TOKEN` (`result`, `comment`, `jwt`). Each `gobj_start()` is a run: it
creates its client, and retires it at its end.

| Property | Value |
|----------|-------|
| **States** | `ST_IDLE` |
| **Output event** | `EV_ON_TOKEN`: `result` 0 and `jwt`, or -1 and `comment` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `issuer` | `string` | OIDC issuer: its `/.well-known/openid-configuration` gives the endpoints. |
| `token_endpoint`, `end_session_endpoint` | `string` | Both given: no discovery. |
| `client_id`, `user_id`, `user_passw` | `string` | The client and the credentials of the password grant. |
| `offline_access` | `boolean` | Ask for an offline token. |
| `crypto` | `dict` | TLS configuration of the calls to the IdP. |

```json
{
    "token_endpoint":       "https://auth.example.com/realms/foo/protocol/openid-connect/token",
    "end_session_endpoint": "https://auth.example.com/realms/foo/protocol/openid-connect/logout",
    "client_id":  "cli",
    "user_id":    "user@example.com",
    "user_passw": "secret"
}
```

**Every failure reaches the caller** as `EV_ON_TOKEN` with `result` -1: a caller
waits for that event and for nothing else. Only the tokens are read of an
answer of the IdP; the rest of the envelope is the IdP's to send or not. A
peer's bad answer is a WARNING, a status other than 200 the `comment`.

**Every run ends**, with the same wait at both places, the `timeout` of
`C_TASK` (10 s by default). Each ends in a log and in `EV_ON_TOKEN` with `result`
-1. A run with no IdP configured (no `issuer`, and only one of `token_endpoint`
and `end_session_endpoint`) does not start: `gobj_start()` answers -1, after
the same `EV_ON_TOKEN`, so subscribe before starting it:

| The IdP | `comment` | Log |
|---------|-----------|-----|
| is not configured | `No IdP configured` | ERROR `task_authenticate: no IdP configured: set 'issuer' or 'token_endpoint'+'end_session_endpoint'` |
| does not accept the connection, or nothing answers at its address | `The IdP cannot be reached` | WARNING `task_authenticate: the IdP cannot be reached` |
| takes a request and does not answer it | `The IdP did not answer` | WARNING `task_authenticate: the IdP did not answer` |

So a caller needs no timer of its own. With nobody at the IdP's address,
`ycommand` gives up after 10 s, exit code 255, and prints the `comment` and
`Abort.`: on stdout when it is verbose (`-l`) or interactive (`-i`), else on
stderr, so the stdout of a script is only ever its answer (up to 7.30.14 it
printed nothing without `-l` or `-i`):

```bash
$ ycommand -l 1 -I https://auth.example.com/realms/foo/ -Z cli -x user -X secret \
    -u wss://node.example.com:1991 -c 'help'

The IdP cannot be reached
Abort.

$ ycommand -I https://auth.example.com/realms/foo/ -Z cli -x user -X secret \
    -u wss://node.example.com:1991 -c 'help' 2>/dev/null; echo $?
255
```

The wait covers the first connection only. An IdP that comes up within it is
used; one that comes up later is not, and the caller starts another run. A
token already published ends the wait for good: an end-session with no answer
ends the run with nothing more to the caller, and one WARNING, *"task_authenticate:
the IdP did not answer the end-session"* (up to 7.30.14 it ended with no log).

**The answer of the end-session.** Any `2xx` ends the session (Keycloak answers
`204`, others `200`). A `4xx` or `5xx` is the IdP's answer, not a fault of the
program: one WARNING, *"task_authenticate: the IdP did not end the session"*,
with `status_code` and the `error` of the IdP's body, for example

```
WARNING task_authenticate: the IdP did not end the session  status_code=400 error=invalid_grant
```

Up to 7.30.14 every status but `204` was an ERROR *"Logout has failed"*, with
the whole answer dumped after it.

**What goes again.** `C_TASK` runs its job again each time the connection opens.
The password grant goes once: a cut is `result` -1 and a WARNING. The discovery
(a GET) and the end-session (idempotent, and not sending it leaves the session
open at the IdP) go twice at most, the second with a WARNING.
