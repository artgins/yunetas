# Auth GClasses

Authentication, authorization, and OAuth 2.

**Source:** `kernel/c/root-linux/src/c_authz.c`, `c_auth_bff.c`,
`c_task_authenticate.c`

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
| `users` / `create-user` / `enable-user` / `disable-user` / `delete-user` | User management. |
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
