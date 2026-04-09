# Performance Benchmarks (C)

Benchmark programs for measuring throughput and latency of Yuneta's core communication and I/O subsystems.

All benchmarks run as self-contained programs: they start an internal server and client, echo messages back and forth, and report throughput metrics. Each benchmark is also registered as a `ctest` target, so `yunetas test` runs them alongside the rest of the test suite.

## Benchmarks

### perf_c_tcp -- TCP Echo (GObject Layer)

**Binaries:** `perf_tcp_test4`, `perf_tcp_test5`

Measures round-trip message throughput over plain TCP using the full GObject protocol stack (`C_IOGATE` -> `C_TCP_S`/`C_TCP` -> `C_PROT_TCP4H` -> `C_CHANNEL`). A `C_PEPON` GObject acts as the echo server. The client sends a ~140-byte JSON message and re-sends every echo it receives, for 180,000 round-trips.

- **GClass:** `C_TEST4` / `C_TEST5`
- **FSM states:** `ST_CLOSED`, `ST_OPENED`
- **Server URL:** `tcp://0.0.0.0:7778`
- **Client URL:** `tcp://127.0.0.1:7778`
- **Message:** `{"id": 1, "tm": 1, "content": "Pepe el alfa.Pepe el alfa...."}`
- **Timeranger2 DB (test5):** `~/tests_yuneta/perf_topic_integer/perf_tcp_test5`

Source files:

| File | Purpose |
|------|---------|
| `main_test4.c` | Yuno entry point, fixed/variable config, GClass registration |
| `main_test5.c` | Same, with timeranger2 persistence config |
| `c_test4.c/h` | GClass C_TEST4 -- plain echo FSM |
| `c_test5.c/h` | GClass C_TEST5 -- echo + tranger2 append |

### perf_c_tcps -- TLS Echo (GObject Layer)

**Binaries:** `perf_tcps_test4`, `perf_tcps_test5`

Identical to `perf_c_tcp` but with TLS encryption enabled. URLs use the `tcps://` scheme. The server is configured with a certificate and key; the client enables TLS without certificate verification.

- **Server URL:** `tcps://0.0.0.0:7778`
- **Client URL:** `tcps://127.0.0.1:7778`
- **TLS library:** OpenSSL or mbedTLS (configurable via Kconfig)
- **Certificate path:** `/yuneta/agent/certs/localhost.crt`, `/yuneta/agent/certs/localhost.key`
- **Timeranger2 DB (test5):** `~/tests_yuneta/perf_topic_integer/perf_tcps_test5`

Source files: same structure as `perf_c_tcp`, with TLS `crypto` config blocks added to the server (`C_TCP_S`) and client (`C_TCP`) service descriptors.

### perf_yev_ping_pong -- Raw Event Loop Ping-Pong

**Binary:** `perf_yev_ping_pong`

Measures the **bare `yev_loop` (io_uring)** throughput with no GObject framework overhead. Creates a TCP server and client using raw `yev_event_h` handles (accept, connect, read, write). The client sends a 1 KB buffer of `'A'` characters; the server echoes it back. Runs for 5 seconds (via `SIGALRM`), printing live Msg/sec and Bytes/sec each second.

- **Server URL:** `tcp://localhost:2222`
- **Buffer size:** 1,024 bytes
- **Duration:** 5 seconds (configurable via `time2exit`)
- **Event types used:** `YEV_ACCEPT_TYPE`, `YEV_CONNECT_TYPE`, `YEV_READ_TYPE`, `YEV_WRITE_TYPE`
- **Drop simulation:** can close client/server/listen fd every N seconds (via `who_drop`, `drop_in_seconds`)

Generates a linker map file (`perf_yev_ping_pong.map`) and assembler listing (`perf_yev_ping_pong.lst`) for low-level profiling.

Source: `src/perf_yev_ping_pong.c` (single file, no GClasses)

### perf_yev_ping_pong2 -- Raw Event Loop + Timeranger2

**Binary:** `perf_yev_ping_pong2`

Same as `perf_yev_ping_pong` with the addition of `tranger2_append_record()` on every received client message, measuring the persistence overhead on the raw event loop.

- **Timeranger2 DB:** `~/tests_yuneta/tr_topic_pkey_integer/topic_pkey_integer_ping_pong`
- **Client message:** JSON `{"hello": "AAA..."}` (serialized via `json2gbuf`)

Source: `src/perf_yev_ping_pong2.c` (single file, no GClasses)

## Performance Summary

### Nov-2024 (RelWithDebInfo)

| Benchmark | Protocol | TLS | Persistence | Throughput |
|-----------|----------|-----|-------------|------------|
| `perf_tcp_test4` | TCP | -- | -- | 35,760 op/sec |
| `perf_tcp_test5` | TCP | -- | timeranger2 | 23,609 op/sec |
| `perf_tcps_test4` | TLS | OpenSSL | -- | 24,862 op/sec |
| `perf_tcps_test5` | TLS | OpenSSL | timeranger2 | 17,406 op/sec |
| `perf_yev_ping_pong` | TCP | -- | -- | 173.3K msg/sec, 177.5 MB/sec |

### Apr-2026 (RelWithDebInfo)

| Benchmark | Protocol | TLS | Persistence | Throughput |
|-----------|----------|-----|-------------|------------|
| `perf_tcp_test4` | TCP | -- | -- | 42,455 op/sec |
| `perf_tcp_test5` | TCP | -- | timeranger2 | 31,438 op/sec |
| `perf_tcps_test4` | TLS | OpenSSL | -- | 30,859 op/sec |
| `perf_tcps_test5` | TLS | OpenSSL | timeranger2 | 24,132 op/sec |
| `perf_yev_ping_pong` | TCP | -- | -- | 167.7K msg/sec, 167.7 MB/sec |
| `perf_yev_ping_pong2` | TCP | -- | timeranger2 | 93.8K msg/sec, 6.5 MB/sec |

### Apr-2026 (Debug)

| Benchmark | Protocol | TLS | Persistence | Throughput |
|-----------|----------|-----|-------------|------------|
| `perf_tcp_test4` | TCP | -- | -- | 28,278 op/sec |
| `perf_tcp_test5` | TCP | -- | timeranger2 | 22,494 op/sec |
| `perf_tcps_test4` | TLS | OpenSSL | -- | 21,514 op/sec |
| `perf_tcps_test5` | TLS | OpenSSL | timeranger2 | 17,746 op/sec |
| `perf_tcps_test4` | TLS | mbedTLS | -- | 13,163 op/sec |
| `perf_tcps_test5` | TLS | mbedTLS | timeranger2 | 11,698 op/sec |
| `perf_yev_ping_pong` | TCP | -- | -- | 159.0K msg/sec, 159.0 MB/sec |
| `perf_yev_ping_pong2` | TCP | -- | timeranger2 | 77.4K msg/sec, 5.4 MB/sec |

### Key takeaways

- **RelWithDebInfo vs Debug:** ~50% higher throughput with optimizations enabled.
- **TLS overhead (OpenSSL):** ~27% overhead relative to plain TCP at the GObject layer (Apr-2026 RelWithDebInfo).
- **TLS backend:** mbedTLS is ~39% slower than OpenSSL for TLS operations (produces ~3x smaller static binaries as trade-off).
- **Timeranger2 persistence:** ~26% overhead on plain TCP, ~22% on TLS (Apr-2026 RelWithDebInfo).
- **Raw `yev_loop` vs GObject stack:** the raw io_uring path is ~4x faster than the full GObject protocol stack for plain TCP echo.
- **ping_pong2 persistence overhead:** timeranger2 append reduces raw loop throughput by ~44% (167.7K → 93.8K msg/sec).

## Building and Running

Benchmarks are built as part of the standard `yunetas build` workflow. Run them directly:

```bash
$YUNETAS_OUTPUTS/bin/perf_yev_ping_pong
```

For TLS benchmarks, certificates must be present at `/yuneta/agent/certs/localhost.crt` and `/yuneta/agent/certs/localhost.key`.

## Directory Structure

```
performance/c/
  CMakeLists.txt                          # adds all subdirectories
  perf_c_tcp/                             # TCP echo (GObj layer)
    CMakeLists.txt
    main_test4.c, c_test4.c, c_test4.h   # plain echo
    main_test5.c, c_test5.c, c_test5.h   # echo + timeranger2
  perf_c_tcps/                            # TLS echo (GObj layer)
    CMakeLists.txt
    main_test4.c, c_test4.c, c_test4.h   # TLS echo
    main_test5.c, c_test5.c, c_test5.h   # TLS echo + timeranger2
  perf_yev_ping_pong/                     # raw io_uring ping-pong
    CMakeLists.txt
    src/perf_yev_ping_pong.c
  perf_yev_ping_pong2/                    # raw io_uring + timeranger2
    CMakeLists.txt
    src/perf_yev_ping_pong2.c
```
