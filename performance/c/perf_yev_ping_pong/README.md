# perf_yev_ping_pong

Ping-pong benchmark for `yev_loop`. Two gobjs exchange TCP messages through the io_uring-backed event loop as fast as possible; the program reports messages/second and bytes/second.

Useful as a smoke test for the event-loop hot path: a regression that halves throughput is immediately visible.

## Run

```bash
$YUNETAS_OUTPUTS/bin/perf_yev_ping_pong
```

For meaningful numbers, build with `CONFIG_BUILD_TYPE_RELEASE=y`. See `perf_yev_ping_pong2` for the variant that adds timeranger2 persistence.
