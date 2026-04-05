# yev_loop test

Integration tests for the io_uring-based event loop: timers, TCP client/server echo, TLS client/server echo (both OpenSSL and mbedTLS), event delivery ordering and clean shutdown.

## Run

```bash
ctest -R test_yev_loop --output-on-failure --test-dir build
```

Benchmarks live under `performance/c/perf_yev_ping_pong*`.
