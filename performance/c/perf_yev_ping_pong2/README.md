# perf_yev_ping_pong2

Updated ping-pong benchmark for `yev_loop`, extended with **timeranger2 persistence**. Every message exchanged is also appended to a timeranger2 topic, so the figures reflect a more realistic workload (event loop + disk I/O).

## Run

```bash
$YUNETAS_OUTPUTS/bin/perf_yev_ping_pong2
```

Compare the throughput against `perf_yev_ping_pong` (pure ping-pong, no persistence) to see the cost of appending each message. Build with `CONFIG_BUILD_TYPE_RELEASE=y` for comparable numbers.
