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
| `test4_discovery_failure` | Discovery body omits `token_endpoint` — `result_save_discovery` rejects it with a WARNING, no ERROR, and publishes `EV_ON_TOKEN` with `result=-1`. |
| `test5_form_encoding` | The user `user+1@x.com` with the password `p+a%41&client_id=evil&grant_type=x`. `C_PROT_HTTP_CL` form-urlencodes each key and value, so the mock (`expected_form`) finds ONE `grant_type`, ONE `client_id`, and the credentials as they are; a mismatch is an ERROR of the mock and a 400. Expects `result=0`. |
| `test6_idp_drops` | The mock (`drop_token_requests`) drops the connection of the first `/token` with no answer: `C_TASK` runs the job again on the new connection, and the task does not send the password grant again: `result=-1`, ONE WARNING. |
| `test7_headers_too_large` | The mock (`raw_token_answer` `@bighead`) answers a header of 70 KB: refused by the HTTP client, `result=-1`, ONE WARNING of the task naming it. |
| `test8_protocol_error` | The mock (`raw_token_answer`) answers `HTTP/1.1 200 OK` and a header name with a space: refused (`protocol_error`), `result=-1`, ONE WARNING. |
| `test9_runs` | The task run three times in one yuno (`set_task_runs(3, FALSE)`): after the third, no `C_PROT_HTTP_CL` and no `C_TCP` left under it (each run retires its client). |
| `test10_stop_in_on_token` | The same, the task stopped inside its `EV_ON_TOKEN` (`set_task_runs(3, TRUE)`), as `ycli` does. |
| `test11_logout_idp_drops` | The mock (`drop_logout_requests`) drops the first `/logout`: the end-session is idempotent, so it goes once more, with a WARNING (`expected_logouts` 2, `set_task_end_delay()`: the logout comes after `EV_ON_TOKEN`). |
| `test12_logout_idp_gone` | The mock drops every `/logout`: 2 arrive, not a third, a WARNING for each, and `result=0`: the token was already published. |
| `test13_unknown_status` | The mock answers `/token` with status 298: `result=-1`. `llhttp_status_name()` aborts on a status it has no name for, and a peer chooses it. |
| `test14_long_bearer` | An access_token of 2000 bytes: the `Authorization` of the `/logout` carries it whole (`expected_bearer_len`). |
| `test15_endpoint_not_url` | The discovery answers a `token_endpoint` that is not a URL: `result=-1`, ONE WARNING. The caller is told: the job used to end with no `EV_ON_TOKEN`. |
| `test16_minimal_answer` | `/token` answers only its tokens: `result=0`, no log. |
| `test17_discovery_cut` | The mock (`drop_discovery_requests`) drops every discovery: 2 go, `result=-1`, a WARNING for each. |
| `test18_idp_keycloak` | `C_IDP_KEYCLOAK` on the same mock, with its own driver (`c_idp_driver.c`: the service and the channel a command answers to) and config (`main_test18_idp_keycloak.c`), 12 commands: what goes again when a connection is cut and what does not, a token too long, an `expires_in` that is a text, negative or too big, a search of 600 bytes. |
| `test19_endpoint_too_long` | The discovery answers a `token_endpoint` of 4096 bytes: `result=-1`, ONE WARNING. |

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
