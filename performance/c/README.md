# performance/c

Performance benchmarks for the C kernel. Each sub-directory is a standalone program that exercises one subsystem under load and reports throughput, latency or both.

| Directory | What it measures |
|---|---|
| `perf_c_tcp` | Raw `C_TCP` throughput (plaintext) |
| `perf_c_tcps` | `C_TCP_S` throughput (TLS, OpenSSL or mbedTLS) |
| `perf_yev_ping_pong` | `yev_loop` ping-pong over a loopback TCP pair |
| `perf_yev_ping_pong2` | ping-pong extended with timeranger2 persistence |

## Running

After `yunetas build`, binaries are installed under `$YUNETAS_OUTPUTS/bin/`. Run them directly:

```bash
$YUNETAS_OUTPUTS/bin/perf_yev_ping_pong
```

Numbers vary with build type (`CONFIG_BUILD_TYPE_RELEASE` gives the most meaningful results) and TLS backend. These programs are primarily regression-detection aids — if a change halves throughput, you'll notice.
