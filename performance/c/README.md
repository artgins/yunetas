# Performance Benchmarks (C)

Benchmark programs for measuring throughput and latency of Yuneta's core communication and I/O subsystems.

All benchmarks run as self-contained programs. The network ones start an internal server and client, echo messages back and forth, and report throughput metrics; the persistence ones (`perf_timeranger2`, `perf_tr_treedb`, `perf_c_treedb`) build a store, measure, print one line of JSON per result, and remove the store; `perf_rotatory` does the same with the log files. Each benchmark is also registered as a `ctest` target, so `yunetas test` runs them alongside the rest of the test suite (the persistence ones with smaller sizes: ctest checks that they build and run; their reference figures come from a run with the default sizes).

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

### perf_auth_bff -- auth_bff Login Round-Trip (HTTP + c_task)

**Binary:** `perf_auth_bff`

Concurrent throughput benchmark for `c_auth_bff`.  Five HTTP client
slots run in parallel against five BFF channels, each slot looping
`connect → POST /auth/login → wait response → close` for 200
iterations (1,000 total round-trips).  Mock Keycloak is configured
with `latency_ms=0` so the bottleneck is the BFF + `c_task` +
`ghttp_parser` + `yev_loop` hot path rather than the simulated
upstream.

- **5 parallel C_AUTH_BFF channels** under `__bff_side__`
  (`[^^children^^]` range expansion, same pattern as the
  production `auth_bff.1801.json` batch but scaled to 5)
- **5 parallel C_MOCK_KEYCLOAK channels** under `__kc_side__`
  with `latency_ms=0`
- **Reuses** `tests/c/c_auth_bff/c_mock_keycloak.{c,h}` and
  `test_helpers.{c,h}` via CMake shared sources — one copy of
  the mock in the repo, linked into ctest, stress, and perf
- `MT_PRINT_TIME` brackets the actual load only (warm-up and
  register phases excluded) so the reported ops/sec reflects
  end-to-end login round-trips

Source: `main_perf_auth_bff.c`, `c_perf_auth_bff.c`

### perf_timeranger2 -- timeranger2 open, topic create, tm query

**Binary:** `perf_timeranger2`

timeranger2 alone: the open of a store of 20 000 md2 files as a master and as a replica, the create of 10 topics and 10 `topic_version` changes, and a tm query of one minute before and after `tranger2_mark_tm_order()`. See [`perf_timeranger2/README.md`](perf_timeranger2/README.md).

Source: `src/perf_timeranger2.c` (single file, no GClasses)

### perf_tr_treedb -- treedb writes

**Binary:** `perf_tr_treedb`

`tr_treedb` over timeranger2, without the gclasses: updates (in memory and saved), links and unlinks, creates, the reopen of the treedb, and forced deletes, 100 000 operations. See [`perf_tr_treedb/README.md`](perf_tr_treedb/README.md).

Source: `src/perf_tr_treedb.c` (single file, no GClasses)

### perf_c_treedb -- C_TREEDB open of a dynamic-schema treedb

**Binary:** `perf_c_treedb`

A yuno that drives `C_TREEDB` through `open-treedb` / `close-treedb` in a store of 40 treedbs x 10 topics x 20 columns: the first projection (seed), a newer literal, and the same literal again. See [`perf_c_treedb/README.md`](perf_c_treedb/README.md).

Source: `src/main.c`, `src/c_perf_treedb_open.c`

### perf_rotatory -- one record of the file log and of the agent audit

**Binary:** `perf_rotatory`

The rotatory alone: 300 000 records of 300 bytes written as the agent audit writes them (two `rotatory_write()` calls), the same with the retention subscribed, the same with a `rotatory_flush()` after each record (what the agent audit does since 7.25.5), and a record with its priority header. See [`perf_rotatory/README.md`](perf_rotatory/README.md).

Source: `src/perf_rotatory.c` (single file, no GClasses)

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

### Sep-2026: 7.25.5 against 7.25.4 (RelWithDebInfo, `CONFIG_DEBUG_TRACK_MEMORY` on)

Each benchmark of this release is linked twice: with the libraries of this
release, and with the module of 7.25.4 compiled with the headers of this
release and put before the libraries (`tr_treedb.c` for `perf_tr_treedb`,
`timeranger2.c` for `perf_timeranger2` and `test_topic_pkey_integer`,
`yev_loop.c` for the network benchmarks, `rotatory.c` for the log files). The
two binaries run alternated, on ext4 on a laptop NVMe, with a `sync` and a
pause before each run. The figures are mean +- standard deviation (median),
and the last column is the change of the mean.

`perf_timeranger2` (ms; 10 rounds, the two append rows 20 rounds after the
change that looks up the key cache once per append):

| Case | 7.25.4 | 7.25.5 | Change |
|------|--------|--------|--------|
| `build_appends` (400 000 appends, 2000 keys x 10 files) | 1733.5 +- 31.2 (1740.3) | 1712.6 +- 32.3 (1701.5) | -1.2% |
| `open_master` (20 000 md2 files) | 92.5 +- 1.9 (91.7) | 80.8 +- 1.9 (80.1) | -12.7% |
| `open_replica` | 89.7 +- 2.1 (88.9) | 106.5 +- 2.0 (105.9) | +18.7% |
| `create_topic` (10 topics) | 1.4 +- 0.0 (1.4) | 159.6 +- 14.2 (167.6) | x114 |
| `topic_version_change` (10) | 8.8 +- 4.0 (10.6) | 276.6 +- 25.0 (286.2) | x31 |
| `tm_build_appends` (600 000 appends, 1 key x 30 files) | 1635.0 +- 29.3 (1642.9) | 1627.3 +- 32.8 (1626.1) | -0.5% |
| `tm_query_unmigrated` (1 key, 30 files x 20 000 rows) | 12.7 +- 0.2 (12.7) | 391.6 +- 8.2 (387.4) | x31 |
| `mark_tm_order` (the same topic) | -- | 19.3 +- 1.5 (19.9) | |
| `tm_query_migrated` | -- | 7.4 +- 0.1 (7.3) | |

`perf_tr_treedb` (us per operation, N = 100 000; 20 rounds, the order of
the two binaries swapped at each round):

| Case | 7.25.4 | 7.25.5 | Change |
|------|--------|--------|--------|
| `update_memory` | 3.88 +- 0.10 (3.89) | 2.89 +- 0.08 (2.88) | -25.4% |
| `update_saved` | 11.45 +- 0.26 (11.54) | 10.36 +- 0.26 (10.38) | -9.6% |
| `link_unlink` | 10.96 +- 0.25 (11.00) | 10.77 +- 0.24 (10.81) | -1.7% |
| `create_link_half` | 69.9 +- 1.4 (69.8) | 70.3 +- 1.1 (70.1) | +0.6% |
| `reopen` (per node) | 391.5 +- 12.8 (388.8) | 389.3 +- 13.4 (388.3) | -0.6% |
| `delete_force` | 66.3 +- 1.4 (66.3) | 67.3 +- 1.3 (67.4) | +1.6% |
| `delete_parent` (per parent of 200 children) | 3061 +- 40 (3053) | 3087 +- 63 (3072) | +0.9% |

`timeranger2/test_topic_pkey_integer` (appends/s, 180 000 appends; 20
rounds, the order swapped at each round):

| Case | 7.25.4 | 7.25.5 | Change |
|------|--------|--------|--------|
| without an rt list | 226 059 +- 4 306 (227 846) | 222 220 +- 3 933 (223 288) | -1.7% |
| with an rt list | 166 927 +- 3 314 (168 580) | 168 555 +- 2 992 (169 793) | +1.0% |

These two binaries do not differ only in `timeranger2.c`. The module of each
release has another size, so every library linked after it (jansson, gbmem)
lands at another address, and the speed of that code changes with its
address. `json_dumps()` is about 40% of the cycles of an append here. With
the same `timeranger2.c` of 7.25.4, only the addresses of the libraries
moved (in steps of 64 bytes), the test goes from 217 557 to 222 951
appends/s. So a difference of 2% between two links is not a difference of
the code.

To remove this effect, each module is padded to the same `.text` size and
linked 4 times, with the libraries moved by 0, 320, 704 and 1472 bytes. 20
rounds of each of the 4 links, all 12 binaries alternated (n = 80 for each
column):

| Case | 7.25.4 | 7.25.5 | Change |
|------|--------|--------|--------|
| without an rt list | 220 120 +- 4 544 (220 070) | 221 588 +- 4 753 (220 424) | +0.7% |
| with an rt list | 166 498 +- 3 354 (165 816) | 167 381 +- 3 306 (166 189) | +0.5% |

Before the change, the same measurement gave 217 720 +- 3 979 (219 238),
-1.1%, and 166 363 +- 3 682 (168 054), -0.1%.

Cycles in `tranger2_append_record()` (`rdtsc` probes in copies of the
module, median of 6 runs, both phases of the test): 7.25.4 14 580, before
the change 15 352, now 14 484. Where the append of 7.25.5 spends differently
(cycles an append, 7.25.4 / before / now):

| Step | 7.25.4 | before | now |
|------|--------|--------|--------|
| master check, topic | 168 | 229 | 203 |
| file id, flagged-file check | 373 | 540 | 450 |
| md2 open, `lseek()`, torn-tail check | 658 | 656 | 668 |
| find the cell, order marker | -- | 577 | 544 |
| cache update (7.25.4: with the find of the cell) | 1 617 | 1 278 | 934 |
| `json_dumps()` (the same code in all three) | 5 784 | 6 002 | 5 791 |

The torn-tail check costs nothing when the md2 ends on a row boundary (one
`%`). Before the change, the key's cache was looked up four times an append
(flagged-file check, find of the cell, cache update, totals). Now it is
looked up once. The `json_dumps()` row shows the layout effect of the
paragraph above: the same code, 218 cycles apart.

yev_loop (8 rounds):

| Benchmark | 7.25.4 | 7.25.5 | Change |
|------|--------|--------|--------|
| `perf_yev_ping_pong` (K msg/s) | 152.2 +- 3.3 (153.7) | 150.5 +- 2.9 (149.8) | -1.1% |
| `perf_tcp_test4` (ops/s) | 38 706 +- 685 (38 841) | 39 344 +- 771 (39 831) | +1.6% |
| `perf_tcp_test5` (ops/s) | 30 421 +- 641 (30 386) | 30 181 +- 597 (30 323) | -0.8% |

rotatory (ns for one record of 300 bytes written with two `rotatory_write()`
calls, as the agent audit writes it; 300 000 records a run, 8 rounds;
`perf_rotatory`, case `audit_record`): 7.25.4
6 194 +- 135 (6 140), 7.25.5 554 +- 10 (555). The agent audit flushes each
record since 7.25.5 (`audit_record_flush`, a busier run): 7.25.4 7 623 +- 1 105
(7 252), 7.25.5 1 475 +- 491 (1 309).

`perf_c_treedb` (seconds for 40 opens). The 7.25.4 columns are the
`c_treedb.c` of 7.25.4 compiled with the headers of 7.25.5 and linked before
the libraries of 7.25.5. With the JSON load of 7.25.4 (`json_loadfd()`, one
`read()` per byte; medians of 4-8 rounds, an earlier run):

| Case | 7.25.4, its JSON load |
|------|--------|
| `seed` | 10.85 |
| `newer_literal` | 13.56 |
| `same_literal` | 1.84 |

With the JSON load of 7.25.5 (read whole, then parsed) on both sides, 10
alternated rounds, mean +- standard deviation (median):

| Case | 7.25.4 | 7.25.5 |
|------|--------|--------|
| `seed` | 9.87 +- 0.42 (9.74) | 10.84 +- 0.32 (10.75) |
| `newer_literal` | 12.79 +- 0.44 (12.86) | 13.89 +- 0.53 (14.02) |
| `same_literal` | 0.672 +- 0.071 (0.647) | 0.613 +- 0.020 (0.610) |

Where an open of 7.25.5 spends differently, from `clock_gettime()` probes
in a copy of `c_treedb.c` (ms an open, means of 3 runs):

| Step | `same_literal` | `newer_literal` | `seed` |
|------|--------|--------|--------|
| second `parse_schema()` of the literal (7.25.4 only) | -2.9 | -3.3 | -2.9 |
| read and parse the schema file in use (60 KB) | +0.7 | +1.3 | -- |
| id-collision check | +0.1 | +0.2 | +0.2 |
| records of an apply / an unfinished projection | +0.04 | +0.1 | +0.06 |
| record of the projection in progress (written whole, fsyncs) | -- | +14.3 | +12.5 |
| index of `__system__` and orphans | -- | +4.9 | +6.6 (with the ownership checks) |
| drafts (`__system__` against the file in use) | -- | +2.9 | -- |
| rest of the projection (ownership checks) | -- | +1.7 | (above) |
| net | -2.0 | +21.9 | +16.5 |

`treedb_open_db()` reads the schema file in use again (as in 7.25.4): the
file is read twice an open, once to decide what runs and once to run it.

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
  perf_auth_bff/                          # auth_bff login round-trip
  perf_timeranger2/                       # timeranger2 open, create, tm query
    CMakeLists.txt
    src/perf_timeranger2.c
  perf_tr_treedb/                         # treedb writes (tr_treedb)
    CMakeLists.txt
    src/perf_tr_treedb.c
  perf_c_treedb/                          # C_TREEDB open (a yuno)
    CMakeLists.txt, small.json
    src/main.c, src/c_perf_treedb_open.c/h
  perf_rotatory/                          # one record of the rotatory (log, audit)
    CMakeLists.txt
    src/perf_rotatory.c
```
