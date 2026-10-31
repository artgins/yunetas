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
| `test5_form_encoding` | The user `user+1@x.com` with the password `p+a%41&client_id=evil&grant_type=x`. `C_PROT_HTTP_CL` form-urlencodes each key and value, so the mock (`expected_form`) finds ONE `grant_type`, ONE `client_id`, and the credentials as they are; a mismatch is an ERROR of the mock and a 400. Expects `result=0`. |
| `test6_idp_drops` | The mock (`drop_token_requests`) drops the connection of the first `/token` with no answer: `C_TASK` runs the job again on the new connection, and the task does not send the password grant again: `result=-1`, ONE WARNING. |
| `test7_headers_too_large` | The mock (`raw_token_answer` `@bighead`) answers a header of 70 KB: refused by the HTTP client, `result=-1`, ONE WARNING of the task naming it. |
| `test8_protocol_error` | The mock (`raw_token_answer`) answers `HTTP/1.1 200 OK` and a header name with a space: refused (`protocol_error`), `result=-1`, ONE WARNING. |

## Mock IdP (`C_MOCK_IDP`)

A trimmed-down sibling of `c_auth_bff/c_mock_keycloak.c`.  Routes by URL
substring:

* `/.well-known/openid-configuration` → synthesises a discovery JSON
  (issuer/token_endpoint/end_session_endpoint) derived from the request
  Host header.  Tests can override the body verbatim via
  `override_discovery_body`.
* `/token` → when `expected_form` (a dict) is set, each of its keys must be
  in the form once, with that value once decoded, else an ERROR and a 400;
  then returns a synthesised token envelope with all the
  `KW_REQUIRED` fields that `result_get_token` reads
  (access_token, refresh_token, expires_in, refresh_expires_in,
  token_type, session_state, scope).  Tokens are plain placeholder
  strings — `c_task_authenticate` does not validate JWT structure or
  signature today, so libjwt is intentionally not a dependency here.
* `/logout` → returns 204.
* `drop_token_requests` drops the connection of that many `/token`
  requests with no answer; `raw_token_answer` answers `/token` with its
  bytes as they are (`@bighead`: a 200 with a header of 70 KB).

## Running

```bash
ctest --test-dir build -R '^c_task_authenticate/' --output-on-failure
```

or one at a time:

```bash
ctest --test-dir build -R '^c_task_authenticate/test1_discovery$' \
    --output-on-failure
```
