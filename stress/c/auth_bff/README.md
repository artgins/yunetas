# stress/c/auth_bff

Concurrent stress runner for the `c_auth_bff` GClass.

## What it does

Spins up a single self-contained yuno with:

- `__bff_side__` IOGate with **5 BFF channels** in parallel (each a
  `C_CHANNEL` + `C_AUTH_BFF` + `C_PROT_HTTP_SR` + `C_TCP` stack).
  Created via the batch `[^^children^^]` range expansion, same
  pattern as `yunos/c/auth_bff/batches/localhost/auth_bff.1801.json`.
- `__kc_side__` IOGate with 5 mock Keycloak channels serving
  `/realms/test/protocol/openid-connect/token` with a short latency
  (`latency_ms=10`) — fast enough for meaningful throughput but not
  instant so the BFF's queue / watchdog / processing lifecycle is
  exercised.
- `c_stress_auth_bff` driver that owns 5 concurrent HTTP client
  slots.  Each slot loops
    `connect → POST /auth/login → wait response → close` for a
  fixed number of iterations (default 20) on its own independent
  BFF channel.

At the end, the driver cross-checks:

- per-slot counters (responses received, status counts)
- aggregated BFF stats across all 5 channels (`requests_total`,
  `kc_calls`, `kc_ok`, `kc_errors`, `bff_errors`, `responses_dropped`,
  `kc_timeouts`, `q_full_drops`)
- aggregated mock KC stats

and fails the binary (via `gobj_log_error` → `capture_log_write`)
if anything doesn't add up.

## Running

```bash
$YUNETAS_OUTPUTS/bin/stress_auth_bff
```

The runner is deterministic; no RNG, no chaos knobs in this first
version.  Re-runs should produce identical counters.

## Shared infrastructure

The binary reuses `c_mock_keycloak.{c,h}` and `test_helpers.{c,h}`
from `tests/c/c_auth_bff/` via CMake `include_directories` and
shared sources — there is only one copy of the mock in the repo.

## Future work

- Chaos knobs: random cancel mid-request, injected KC errors,
  random KC latency spikes
- CLI args for num_clients / iterations / seed (currently
  compile-time defines)
- Results directories like `stress/c/listen/results-*`
- Memory / FD leak tracking between iterations
