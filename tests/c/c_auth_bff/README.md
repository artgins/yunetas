# tests/c/c_auth_bff

Self-contained correctness tests for the `c_auth_bff` GClass.

## Design

Each test spawns a single yuno that hosts three side-by-side services:

```
  c_test<N>  (driver — orchestrates the sequence, validates)
  __bff_side__      C_IOGATE + C_TCP_S(:BFF_PORT) + C_CHANNEL + C_PROT_HTTP_SR + C_AUTH_BFF
                    crypto.url → http://127.0.0.1:KC_PORT/realms/test/protocol/openid-connect/token
  __idp_side__       C_IOGATE + C_TCP_S(:KC_PORT) + C_CHANNEL + C_PROT_HTTP_SR + C_MOCK_KEYCLOAK
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
| `test_auth_bff_test8_queue_full` | Pipelines 4 `POST /auth/login` requests on a single TCP connection against a BFF with `pending_queue_size=2` and mock KC `latency_ms=300`. The 4th POST hits the queue-full branch in `enqueue()` and is rejected with 503; the first 3 are served after staged KC round-trips. Asserts `requests_total=3, kc_calls=3, kc_ok=3, bff_errors=1, q_full_drops=1` and mock KC `token_requests=3, responses_sent=3`. The only test that exercises HTTP/1.1 pipelining on the driver side. |
| `test_auth_bff_test9_browser_cancel` | **Main sporadic-failure reproducer**: browser aborts its HTTP connection while the BFF is still waiting for a Keycloak reply. Mock KC `latency_ms=500`; driver POSTs `/auth/login`, then at t+100 ms calls `gobj_stop_tree` on its http_cl. 800 ms later the mock KC reply arrives at the BFF — which must drop it silently (via the `browser_alive` gate in `c_auth_bff::result_token_response`). Asserts BFF stats `kc_ok=1, bff_errors=0, responses_dropped=1` and mock KC `responses_sent=1, pending_cancelled=0`. Uses `persistent_channels=true` on `__bff_side__` so the BFF instance survives the cancel long enough to run `result_token_response` and exercise the gate. |
| `test_auth_bff_test10_kc_silence` | **Deadlock reproducer**: Keycloak accepts the TCP connection but never responds. Mock KC `latency_ms=60000`, BFF `idp_timeout_ms=500`. Without the watchdog fix, the BFF would wedge until `auto_kill_time` expired; with it, the BFF fires `ac_kc_watchdog` at 500 ms, sends 504 to the browser via `send_error_response`, then drains the task through the normal `ac_end_task` cleanup. Asserts BFF stats `kc_timeouts=1, bff_errors=1, kc_ok=0` and mock KC `responses_sent=0, pending_cancelled=1`. |
| `test_auth_bff_test11_cancel_retry` | Follow-up to test9: validates that BFF state is cleanly re-initialised after a cancel. Driver POSTs `/auth/login`, cancels at 100 ms, waits 800 ms (for the stale reply to drain through the gate), then opens a second TCP connection to the same channel and POSTs `/auth/login` again. Second request must succeed cleanly (`kc_ok=2, responses_dropped=1`). Catches leaks in `q_count`, `priv->gobj_http`, `priv->processing`, and proves `browser_alive` correctly flips back to TRUE on the second `EV_ON_OPEN`. |

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
