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
| `list-jwk` / `add-jwk` / `remove-jwk` | Manage JSON Web Keys. |
| `users` / `create-user` / `update-user` / `enable-user` / `disable-user` / `delete-user` | User management. A `role` given to `create-user` / `update-user` is a ref `roles^<role id>^users` to a role that exists; anything else is refused before the user is written (*"Role does not exist"*, *"Bad role ref, expected roles^ROLE^users"*), and the user keeps the roles it had. Example: `ycommand -c 'command-yuno id=<id> service=authz command=update-user username=ana@example.com role=roles^operator^users'`. |
| `accesses` | List access rules. |

---

(gclass-c-auth-bff)=
## C_AUTH_BFF

Backend-For-Frontend OAuth 2 server — mediates between browser SPAs and
Keycloak, storing tokens in httpOnly cookies.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE`, `ST_WAIT_RESPONSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `keycloak_url` | `string` | Keycloak server URL. |
| `realm` | `string` | Keycloak realm name. |
| `client_id` | `string` | OAuth 2 client ID. |
| `client_secret` | `string` | OAuth 2 client secret. |
| `cookie_domain` | `string` | Domain for httpOnly cookies. |
| `allowed_origin` | `string` | CORS allowed origin. |
| `allowed_redirect_uri` | `string` | Allowed redirect URI after login. |
| `crypto` | `json` | TLS configuration. |

### Endpoints

| Endpoint | Description |
|----------|-------------|
| `POST /auth/login` | Start login flow. |
| `POST /auth/callback` | Handle OAuth callback. |
| `POST /auth/refresh` | Refresh access token. |
| `POST /auth/logout` | Logout and clear cookies. |

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
| `register-idp-user` | `register-idp-user` | Create an account (`email` required, used as username; `firstName`, `lastName`). The user gets the email to set the password. Publishes `EV_IDP_USER_CREATED`. `role` is legacy: roles belong to the authorization plane, which applies its `default_role`. |
| `list-idp-users` | `manage-idp-users` | List the accounts: `search` (username, names, email), paging `first` / `max` (default 50, ceiling 500), `brief=0` for the full representation. |
| `get-idp-user` | `manage-idp-users` | One account, by `user_id` (the uuid Keycloak gives, not the email). |
| `update-idp-user` | `manage-idp-users` | Change `firstName`, `lastName`, `enabled`, `emailVerified`, `requiredActions`; an absent parameter is left unchanged. |
| `delete-idp-user` | `manage-idp-users` | Delete the account in the IdP. The local authz record is not touched. |
| `send-idp-user-actions` | `manage-idp-users` | Email the user the link to run `actions` (default `["UPDATE_PASSWORD"]`; e.g. `VERIFY_EMAIL`). |

`user_id` also answers to `id`, which is unusable through `command-yuno`
(the agent reads `id` as the yuno to address).

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
ycommand -c "command-yuno id=1620 service=idp command=set-kc-config \
    kc_base_url=https://auth.example.com kc_realm=example \
    kc_admin_client_id=yuneta-admin kc_admin_client_secret=<secret> \
    kc_redirect_uri=https://app.example.com/ kc_email_client_id=app"
ycommand -c "command-yuno id=1620 service=idp command=register-idp-user email=ana@example.com firstName=Ana"
ycommand -c "command-yuno id=1620 service=idp command=list-idp-users search=ana"
```

Details of the provisioning flow: [`YUNO_AUTH.md`](../../../../yunos/c/yuno_agent/YUNO_AUTH.md).

---

(gclass-c-task-authenticate)=
## C_TASK_AUTHENTICATE

OAuth 2 authentication task — handles the Keycloak authentication flow
and caches tokens.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_DISCONNECTED`, `ST_WAIT_CONNECTED`, `ST_WAIT_RESPONSE`, `ST_AUTHENTICATED` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | Keycloak token endpoint URL. |
| `jwt` | `string` | Cached JWT token (read-only). |
