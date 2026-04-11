# tests/c/c_auth_bff

Self-contained correctness tests for the `c_auth_bff` GClass.

## Design

Each test spawns a single yuno that hosts three side-by-side services:

```
  c_test<N>  (driver — orchestrates the sequence, validates)
  __bff_side__      C_IOGATE + C_TCP_S(:BFF_PORT) + C_CHANNEL + C_PROT_HTTP_SR + C_AUTH_BFF
                    crypto.url → http://127.0.0.1:KC_PORT/realms/test/protocol/openid-connect/token
  __kc_side__       C_IOGATE + C_TCP_S(:KC_PORT) + C_CHANNEL + C_PROT_HTTP_SR + C_MOCK_KEYCLOAK
```

The test driver (`C_TEST<N>`) creates a transient `C_PROT_HTTP_CL` pointing at
`__bff_side__`, POSTs to the BFF's public endpoints, and inspects the response
— status code, `Set-Cookie` headers, body fields, and finally the BFF's own
`gobj_stats()` snapshot as a cross-check.

`C_MOCK_KEYCLOAK` is a small HTTP service that mimics Keycloak's token and
logout endpoints.  It reads its response behaviour from attrs (return status,
canned username/email, latency, error injection, force drop) so that each
test scripts the server exactly.  Tokens are real HS256 JWTs signed with a
fixed test key so the BFF's payload decode sees a well-formed token.

## Tests

| Binary | What it exercises |
|--------|--------------------|
| `test_auth_bff_test1_login` | Happy path `POST /auth/login`. BFF calls mock KC, mock KC returns a signed token, BFF responds 200 with `Set-Cookie` + body `{username, email}`. Asserts `requests_total=1, kc_calls=1, kc_ok=1, bff_errors=0`. |
| `test_auth_bff_test2_kc_401` | Mock KC returns 401 on `/token`. BFF must forward as HTTP 400, body `{success:false, error}`. Asserts `kc_calls=1, kc_ok=0, kc_errors=1, bff_errors=1`. |
| `test_auth_bff_test3_callback` | Happy path `POST /auth/callback` with PKCE `{code, code_verifier, redirect_uri}`. BFF `allowed_redirect_uri` active. Asserts the same success profile as test1. |
| `test_auth_bff_test4_refresh` | Happy path `POST /auth/refresh`. Cookie header carries a fake `refresh_token`; empty JSON body. Response has `{success, expires_in, refresh_expires_in}` — no username/email. |
| `test_auth_bff_test5_logout` | Happy path `POST /auth/logout`. Mock KC returns the spec-compliant 204 No Content; BFF stats (post-fix) count 2xx as `kc_ok`. Browser-facing response is 200 + clear-cookie `Set-Cookie` headers. |
| `test_auth_bff_test6_invalid_body` | Negative: `POST /auth/login` with a body missing `password`. BFF must reject with 400 *before* touching the queue or Keycloak. Asserts `kc_calls=0, bff_errors=1`. |
| `test_auth_bff_test7_slow_login` | Happy path against a **slow** mock Keycloak (`latency_ms=200`). The mock's new deferred-response path holds each `/token` reply on an internal `C_TIMER` child for 200 ms. Asserts both the BFF stats (same profile as test1) **and** the mock KC's own stats (`deferred_responses=1, pending_cancelled=0`). First test that exercises the latency code path end-to-end — prerequisite for F2 browser-cancel / KC-silence tests. |

Shared infrastructure:

- `c_mock_keycloak.{c,h}` — scripted Keycloak mock (signed HS256 JWTs,
  per-request behaviour scripted via attrs).
- `test_helpers.{c,h}` — `test_helpers_find_bff`, `test_helpers_check_stats`
  and the `TEST_HELPERS_BEGIN_DYING` graceful-shutdown macro used by
  every driver from test2 onwards. test1 predates the helper file and
  still inlines the same logic.

## Running

```bash
ctest -R c_auth_bff --output-on-failure --test-dir build
```

or a single binary:

```bash
$YUNETAS_OUTPUTS/bin/test_auth_bff_test1_login
```
