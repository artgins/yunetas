# Benchmarks & Stress Tests

Programs for measuring throughput, latency, and stability under load.

**Source:** `performance/c/`, `stress/c/`

## Performance benchmarks

Measure throughput and latency of specific subsystems. All performance
binaries are registered as `ctest` targets and run automatically with
`yunetas test`.

| Binary | Description |
|--------|-------------|
| **`perf_tcp_test4`** | TCP echo throughput through the GObj protocol stack (plain). ~35,760 op/sec. |
| **`perf_tcp_test5`** | Same as test4 but with timeranger2 persistence. ~23,609 op/sec. |
| **`perf_tcps_test4`** | TLS-encrypted TCP echo throughput. ~24,862 op/sec. |
| **`perf_tcps_test5`** | TLS + timeranger2 persistence. ~17,406 op/sec. |
| **`perf_yev_ping_pong`** | Raw io_uring ping-pong (no GObj overhead). ~173K msg/sec, 177 MB/sec. |
| **`perf_yev_ping_pong2`** | Same as ping-pong but including timeranger2 persistence overhead. |

**Source:** `performance/c/perf_c_tcp/`, `performance/c/perf_c_tcps/`,
`performance/c/perf_yev_ping_pong/`, `performance/c/perf_yev_ping_pong2/`

## Stress tests

Test scalability and stability under sustained extreme load. Stress tests
are run manually — they are **not** part of `ctest`.

| Binary | Description |
|--------|-------------|
| **`stress_listen`** | TCP server that accepts massive concurrent connections (tested up to 1.5M). Tracks connection/message metrics and resource usage. Architecture: `C_LISTEN` → `C_IOGATE` → `C_TCP_S` → 11,000 pre-allocated `C_CHANNEL` → `C_PROT_TCP4H` → `C_TCP`. |

Companion Node.js scripts for generating load:

| Script | Description |
|--------|-------------|
| `stress-connections.js` | Connection load generator. |
| `stress-traffic.js` | Message traffic generator. |

**Source:** `stress/c/listen/`

## Build flags

Both directories are enabled by default via CMake options in the root
`CMakeLists.txt`:

```cmake
option(ENABLE_PERFORMANCE "Build performance" ON)
option(ENABLE_STRESS      "Build stress"      ON)
```

Pass `-DENABLE_PERFORMANCE=OFF` or `-DENABLE_STRESS=OFF` to skip them.
