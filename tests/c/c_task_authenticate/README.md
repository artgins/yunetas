# tests/c/c_task_authenticate

Self-contained unit tests for `kernel/c/root-linux/src/c_task_authenticate.c`,
the OIDC ROPC task GClass.

The suite mirrors the structure of `tests/c/c_auth_bff` but is much
slimmer because every assertion ultimately reduces to *"did the task
publish `EV_ON_TOKEN` with the expected `result`?"*.  Shared scaffolding
(yuno boilerplate, log-capture handler, mock IdP) lives in
`test_main.c`, `c_mock_idp.c` and `c_test_driver.c`; each
`main_test*.c` is just a few lines that pick the IdP attrs to set, the
expected `EV_ON_TOKEN.result`, and the expected log messages.

## Tests

| Binary | What it covers |
|---|---|
| `test1_discovery` | Happy path with `issuer` only — exercises `action_discover` + `result_save_discovery` + `action_get_token` + `action_logout`. |
| `test2_explicit_endpoints` | `token_endpoint` + `end_session_endpoint` set explicitly — discovery is skipped, the chain runs `action_get_token` + `action_logout` directly. |
| `test4_discovery_failure` | Discovery body omits `token_endpoint` — `result_save_discovery` rejects it, logs an error, and publishes `EV_ON_TOKEN` with `result=-1`. |

## Mock IdP (`C_MOCK_IDP`)

A trimmed-down sibling of `c_auth_bff/c_mock_keycloak.c`.  Routes by URL
substring:

* `/.well-known/openid-configuration` → synthesises a discovery JSON
  (issuer/token_endpoint/end_session_endpoint) derived from the request
  Host header.  Tests can override the body verbatim via
  `override_discovery_body`.
* `/token` → returns a synthesised token envelope with all the
  `KW_REQUIRED` fields that `result_get_token` reads
  (access_token, refresh_token, expires_in, refresh_expires_in,
  token_type, session_state, scope).  Tokens are plain placeholder
  strings — `c_task_authenticate` does not validate JWT structure or
  signature today, so libjwt is intentionally not a dependency here.
* `/logout` → returns 204.

## Running

```bash
ctest --test-dir build -R 'c_task_authenticate' --output-on-failure
```

or one at a time:

```bash
ctest --test-dir build -R 'test_task_authenticate_test1_discovery' \
    --output-on-failure
```
