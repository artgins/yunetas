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

Currently:

| Binary | What it exercises |
|--------|--------------------|
| `test_auth_bff_test1_login` | Happy path POST `/auth/login` — BFF calls mock KC, mock KC returns a signed token, BFF responds 200 with `Set-Cookie` for access/refresh tokens and body containing `username` and `email`. Validates the response and the BFF's own stats counters (`requests_total=1`, `kc_calls=1`, `kc_ok=1`, `bff_errors=0`). |

More tests land in subsequent commits (validation errors, queue full,
browser cancel, cancel-retry — see the F1/F2 plan in the hilo auth_bff).

## Running

```bash
ctest -R c_auth_bff --output-on-failure --test-dir build
```

or a single binary:

```bash
$YUNETAS_OUTPUTS/bin/test_auth_bff_test1_login
```
