# Performance Benchmarks

Benchmark programs for measuring throughput and latency of Yuneta's core communication and I/O subsystems.

All benchmarks run as self-contained programs: they start an internal server and client, echo messages back and forth, and report throughput metrics. Each benchmark is also registered as a `ctest` target, so `yunetas test` runs them alongside the rest of the test suite.

## Benchmarks

### perf_c_tcp -- TCP Echo (GObject Layer)

**Binaries:** `perf_tcp_test4`, `perf_tcp_test5`

Measures round-trip message throughput over plain TCP using the full GObject protocol stack (`C_IOGATE` -> `C_TCP_S`/`C_TCP` -> `C_PROT_TCP4H` -> `C_CHANNEL`). A `C_PEPON` GObject acts as the echo server. The client sends a ~140-byte JSON message and re-sends every echo it receives, for 180,000 round-trips.

| Variant | What it adds | Recorded throughput |
|---------|-------------|---------------------|
| **test4** | Plain echo | 35,760 op/sec |
| **test5** | Echo + `tranger2_append_record()` per message | 23,609 op/sec |

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

Identical to `perf_c_tcp` but with TLS encryption enabled via OpenSSL. URLs use the `tcps://` scheme. The server is configured with a certificate and key; the client enables OpenSSL without certificate verification.

| Variant | What it adds | Recorded throughput |
|---------|-------------|---------------------|
| **test4** | TLS echo | 24,862 op/sec |
| **test5** | TLS echo + `tranger2_append_record()` per message | 17,406 op/sec |

- **Server URL:** `tcps://0.0.0.0:7778`
- **Client URL:** `tcps://127.0.0.1:7778`
- **TLS library:** OpenSSL (configurable via Kconfig)
- **Certificate path:** `/yuneta/agent/certs/localhost.crt`, `/yuneta/agent/certs/localhost.key`
- **Timeranger2 DB (test5):** `~/tests_yuneta/perf_topic_integer/perf_tcps_test5`

Source files: same structure as `perf_c_tcp`, with TLS `crypto` config blocks added to the server (`C_TCP_S`) and client (`C_TCP`) service descriptors.

### perf_yev_ping_pong -- Raw Event Loop Ping-Pong

**Binary:** `perf_yev_ping_pong`

Measures the **bare `yev_loop` (io_uring)** throughput with no GObject framework overhead. Creates a TCP server and client using raw `yev_event_h` handles (accept, connect, read, write). The client sends a 1 KB buffer of `'A'` characters; the server echoes it back. Runs for 5 seconds (via `SIGALRM`), printing live Msg/sec and Bytes/sec each second.

| Metric | Recorded value |
|--------|---------------|
| **Msg/sec** | 173.3 K |
| **Bytes/sec** | 177.5 MB |

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

Results recorded 18-Nov-2024 (17-Nov-2024 for ping-pong) on the developer's machine:

| Benchmark | Protocol | Persistence | Throughput | vs. baseline |
|-----------|----------|-------------|------------|--------------|
| `perf_tcp_test4` | TCP | -- | 35,760 op/sec | baseline (GObj) |
| `perf_tcp_test5` | TCP | timeranger2 | 23,609 op/sec | -34% |
| `perf_tcps_test4` | TLS | -- | 24,862 op/sec | -30% |
| `perf_tcps_test5` | TLS | timeranger2 | 17,406 op/sec | -51% |
| `perf_yev_ping_pong` | TCP | -- | 173.3K msg/sec, 177.5 MB/sec | baseline (raw loop) |
| `perf_yev_ping_pong2` | TCP | timeranger2 | ~173K msg/sec | see above |

Key takeaways:
- TLS adds ~30% overhead relative to plain TCP at the GObject layer.
- Timeranger2 disk persistence adds ~34% overhead on plain TCP, ~30% on TLS.
- The raw `yev_loop` (io_uring) path is ~4.8x faster than the full GObject protocol stack for plain TCP echo, demonstrating the cost of the GObj FSM + protocol framing layers.

## Building and Running

Benchmarks are built as part of the standard `yunetas build` workflow. They live in `performance/c/` and are compiled via CMake.

```bash
# Build everything (includes performance binaries)
yunetas build

# Run all tests including benchmarks
yunetas test

# Run a specific benchmark directly
./build/performance/c/perf_yev_ping_pong/perf_yev_ping_pong
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
