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
| **`perf_auth_bff`** | Ping-pong-style live throughput over the OAuth2 BFF ([`C_AUTH_BFF`](#gclass-c-auth-bff)). Default 10 s runs, ~180,000 ops on the reference box. |
| **`perf_timeranger2`** | timeranger2: the appends that build a store of 20 000 md2 files, its open as a master and as a replica, the create of topics and a `topic_version` change, a tm query before and after `tranger2_mark_tm_order()`. Open as a master: ~81 ms. |
| **`perf_tr_treedb`** | treedb writes without the gclasses: updates in memory and saved, links and unlinks, creates, the reopen, forced deletes (of a child, and of a parent with 200 children). An update in memory: ~2.9 us. |
| **`perf_c_treedb`** | The open of a dynamic-schema treedb by `C_TREEDB` in a store of 40 treedbs: the same literal, a newer literal, the first projection (seed). |
| **`perf_rotatory`** | The rotatory (the file log of every yuno and the agent audit): one record of 300 bytes as the audit writes it, with the retention, with a flush after each record, and one log record with its header. An audit record: ~0.55 us. |

The last four print one line of JSON per result, for example:

```json
{"bench": "perf_tr_treedb", "case": "update_memory", "seconds": 0.289000, "ops": 100000, "us_per_op": 2.890, "events": 0}
```

`perf_timeranger2 --small`, `perf_tr_treedb --small`, `perf_rotatory --small`
and `perf_c_treedb --config-file=small.json` run them with the sizes ctest uses. The figures of each release,
against the release before, are in `performance/c/README.md`.

**Source:** `performance/c/perf_c_tcp/`, `performance/c/perf_c_tcps/`,
`performance/c/perf_yev_ping_pong/`, `performance/c/perf_yev_ping_pong2/`,
`performance/c/perf_auth_bff/`, `performance/c/perf_timeranger2/`,
`performance/c/perf_tr_treedb/`, `performance/c/perf_c_treedb/`,
`performance/c/perf_rotatory/`

## Stress tests

Test scalability and stability under sustained extreme load. Stress tests
are run manually — they are **not** part of `ctest`.

| Binary | Description |
|--------|-------------|
| **`stress_listen`** | TCP server that accepts massive concurrent connections (tested up to 1.5M). Tracks connection/message metrics and resource usage. Architecture: `C_LISTEN` → [`C_IOGATE`](#gclass-c-iogate) → [`C_TCP_S`](#gclass-c-tcp-s) → 11,000 pre-allocated [`C_CHANNEL`](#gclass-c-channel) → [`C_PROT_TCP4H`](#gclass-c-prot-tcp4h) → [`C_TCP`](#gclass-c-tcp). |
| **`stress/auth_bff`** | Drives concurrent OAuth2 BFF login / refresh / logout cycles to expose races between the pending queue, the `kc_timeout` watchdog and the flush-on-disconnect path. |

Companion Node.js scripts for generating load:

| Script | Description |
|--------|-------------|
| [`stress-connections.js`](https://github.com/artgins/yunetas/blob/7.25.5/stress/c/listen/stress-connections.js) | Connection load generator. |
| [`stress-traffic.js`](https://github.com/artgins/yunetas/blob/7.25.5/stress/c/listen/stress-traffic.js) | Message traffic generator. |

**Source:** `stress/c/listen/`, `stress/c/auth_bff/`

## Build flags

Both directories are enabled by default via CMake options in the root
`CMakeLists.txt`:

```cmake
option(ENABLE_PERFORMANCE "Build performance" ON)
option(ENABLE_STRESS      "Build stress"      ON)
```

Pass `-DENABLE_PERFORMANCE=OFF` or `-DENABLE_STRESS=OFF` to skip them.
