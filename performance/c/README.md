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
and the last column is the change of the mean. Every figure of this section
was measured with `CONFIG_DEBUG_TRACK_MEMORY` on (the `.config` of the build
machine); the older sections above do not say how their binaries were built.

`perf_timeranger2` (ms; 10 rounds, the two append rows 20 rounds; the
`create_topic` and `topic_version_change` rows 20 rounds of each of the 4
padded links described below, n = 80, measured again on 2026-09-24):

| Case | 7.25.4 | 7.25.5 | Change |
|------|--------|--------|--------|
| `build_appends` (400 000 appends, 2000 keys x 10 files) | 1733.5 +- 31.2 (1740.3) | 1712.6 +- 32.3 (1701.5) | -1.2% |
| `open_master` (20 000 md2 files) | 92.5 +- 1.9 (91.7) | 80.8 +- 1.9 (80.1) | -12.7% |
| `open_replica` | 89.7 +- 2.1 (88.9) | 106.5 +- 2.0 (105.9) | +18.7% |
| `create_topic` (10 topics) | 3.6 +- 1.3 (4.3) | 118.9 +- 13.1 (115.9) | x33 |
| `topic_version_change` (10) | 1.7 +- 0.9 (1.4) | 231.5 +- 23.4 (224.2) | x136 |
| `tm_build_appends` (600 000 appends, 1 key x 30 files) | 1635.0 +- 29.3 (1642.9) | 1627.3 +- 32.8 (1626.1) | -0.5% |
| `tm_query_unmigrated` (1 key, 30 files x 20 000 rows) | 12.7 +- 0.2 (12.7) | 391.6 +- 8.2 (387.4) | x31 |
| `mark_tm_order` (the same topic) | -- | 19.3 +- 1.5 (19.9) | |
| `tm_query_migrated` | -- | 7.4 +- 0.1 (7.3) | |

The last fixes of timeranger2 (a key or a topic that cannot be listed, a
failed backup: commits 4538d0a5b and 8702a472e), measured as an A/B of that
change alone, 10 alternated rounds, on a machine shared with other builds
(ms, mean +- standard deviation):

| Case | before | after | Change |
|------|--------|-------|--------|
| `build_appends` | 1927 +- 81 | 1874 +- 68 | -2.8% |
| `open_master` | 89.1 +- 6.2 | 87.1 +- 3.6 | -2.2% |
| `open_replica` | 117.1 +- 6.5 | 116.0 +- 4.1 | -0.9% |

And the retry of a flag at each load and at each notification (commit
9d43b317f), 6 alternated rounds, change of the mean: `open_master` +0.8%,
`open_replica` +0.4%, `tm_query_migrated` -1.4%, `tm_query_unmigrated` -2.8%,
`build_appends` +0.1%, `mark_tm_order` +1.4%. Every difference is inside the
noise; with nothing flagged the retry costs two dict lookups per iterator open
and one per notification.

The fixes after them (a failed `delete_key` that indexes its iterators again,
a key listed again only when its directory changed, a backup whose create
fails moved back: commit 2b2288082), as an A/B of that change alone with the
same compile flags, 8 alternated rounds (ms, mean +- standard deviation):

| Case | before | after | Change |
|------|--------|-------|--------|
| `build_appends` | 1517.6 +- 24.3 | 1526.8 +- 14.5 | +0.6% |
| `open_master` | 83.1 +- 0.7 | 83.3 +- 0.7 | +0.2% |
| `open_replica` | 136.5 +- 2.3 | 136.2 +- 1.8 | -0.2% |

Noise. The whole-or-nothing create (commit 5eae26c6d) came after that run: it
changes only the failure paths of a create.

The directory walks that fail on a path longer than PATH_MAX or on a
transient error, and the queue that takes its topic again after a failed
backup (commits 3573b4cd3 and 05812cbe4), 8 alternated rounds, the binaries
linked against a copy of the libraries taken before the change (ms, mean +-
standard deviation):

| Case | before | after | Change |
|------|--------|-------|--------|
| `open_master` | 81.17 +- 2.99 | 81.90 +- 5.05 | +0.9% |
| `open_replica` | 106.47 +- 2.04 | 106.36 +- 0.95 | -0.1% |

Noise: an open walks the key directories as before, with one test of the
length of each path.

`perf_tr_treedb` (CPU us per operation, N = 100 000; 14 rounds x 4 link
layouts, paired alternated runs, `taskset -c 6`):

| Case | 7.25.4 | 7.25.5 | Change |
|------|--------|--------|--------|
| `update_memory` | 3.92 +- 0.08 (3.94) | 2.92 +- 0.53 (2.79) | -25.6% |
| `update_saved` | 11.75 +- 0.42 (11.73) | 10.81 +- 1.51 (10.44) | -8.0% |
| `link_unlink` | 11.27 +- 0.27 (11.20) | 11.09 +- 1.24 (10.85) | -1.6% |
| `create_link_half` | 53.92 +- 1.88 (53.55) | 54.26 +- 7.47 (52.60) | +0.6% (medians -1..-2%) |
| `reopen` (per node) | 396.6 +- 13.8 (395.5) | 401.9 +- 51.4 (394.4) | +1.3% (medians -1.8..+1.0%) |
| `delete_force` | 53.12 +- 3.17 (52.65) | 54.05 +- 5.50 (53.20) | +1.7% |
| `delete_parent` (per parent of 200 children) | 2952 +- 375 (2896) | 2781 +- 427 (2711) | -5.8% |

The delete guard that counts every child a hook holds instead of building a
reference string for each (commit 2ab843d1d), as an A/B of that change alone
(only `tr_treedb.o` differs; 12 rounds x 4 layouts, alternated, `taskset -c
6`, N = 100 000, CPU us per operation, mean +- standard deviation (median);
another day and a busier machine, so the absolute figures are higher than in
the table above):

| Case | before | after | Paired change |
|------|--------|-------|---------------|
| `delete_force` | 71.33 +- 3.47 (71.41) | 70.86 +- 3.49 (69.89) | -0.50% +- 0.84% |
| `delete_parent` | 3253 +- 486 (3139, one outlier) | 3085 +- 123 (3076) | -3.85% +- 1.43% |

The paired change is the mean of the 48 ratios after/before, +- its standard
error. For `delete_parent` it is pulled by one outlier round of the "before"
binary in the fourth layout (the paired mean of that layout is -9.9%); the
median of the 48 ratios is -1.6%. The median of each layout moved -0.7%,
-2.6%, +1.7% and -4.8%, and the median of all the runs -2.0% (3139 -> 3076):
`delete_parent` is ~2% faster, because the per-child strings are no longer
built.

The delete that looks at every instance of its key, and the save guard that
refuses a node no index holds (commit 60f3a4e5e), as an A/B of that change
alone (only `tr_treedb.o` differs; 12 rounds x 4 layouts, alternated,
`taskset -c 6`, CPU us per operation; the change is the mean of the 48
paired ratios, then their median):

| Case | before | after | Paired change (median) |
|------|--------|-------|---------------|
| `update_memory` (no save, control) | 3.13 | 3.14 | +0.8% (+0.9%) |
| `update_saved` | 11.44 | 11.61 | +1.7% (+0.8%) |
| `link_unlink` | 11.81 | 12.09 | +2.6% (+0.6%) |
| `create_link_half` | 60.48 | 60.43 | +0.5% (-0.3%) |
| `reopen` | 432.7 | 430.3 | -0.1% (+0.7%) |
| `delete_force` | 58.72 | 60.64 | +3.5% (+2.5%) |
| `delete_parent` | 3058 | 3074 | +1.1% (+1.5%) |

Each pair varies by 8-13% on that shared machine: none of these moves is
significant. `delete_force` pays one array of the key's instances per delete.
The topics of `perf_tr_treedb` have no pkey2.

The load that puts the primary in its own pkey2 slot instead of a copy
(commit a2b136144): `perf_tr_treedb`, 8 rounds x 4 layouts, moves -1.5%
(`delete_parent`) .. +0.5% (`reopen`), noise. A benchmark written for the
change (not in the tree: 4000 keys x 3 instances, a timed reopen, then 4000
updates through `treedb_get_instance()`; 12 alternated rounds, mean +-
standard deviation):

| Case | before | after | Change |
|------|--------|-------|--------|
| reopen (ms) | 353.5 +- 16.8 | 327.2 +- 8.7 | -7.4% |
| update (us) | 29.4 +- 1.5 | 28.6 +- 1.3 | -2.7% |

The reopen no longer builds the copies.

The save guard that refuses a node whose pkey2 value was changed in place,
and the create that indexes the value its record holds (commit 2d98df267),
as an A/B of that change alone (only `tr_treedb.o` differs; 20 alternated
rounds, CPU us per operation, median (mean +- standard deviation)):

| Case | before | after | Change of the median |
|------|--------|-------|--------|
| `update_memory` (no save) | 2.914 (2.922 +- 0.063) | 3.069 (3.072 +- 0.105) | +5.3% |
| `update_saved` | 10.805 (10.869 +- 0.367) | 10.836 (11.019 +- 0.583) | +0.3% |
| `link_unlink` | 11.062 (11.186 +- 0.432) | 11.015 (11.231 +- 0.529) | -0.4% |
| `create_link_half` | 72.209 (73.499 +- 3.716) | 71.727 (72.567 +- 2.514) | -0.7% |
| `reopen` | 403.6 (408.2 +- 16.7) | 408.0 (407.3 +- 12.9) | +1.1% |
| `delete_force` | 69.95 (70.10 +- 2.47) | 69.17 (72.76 +- 11.68) | -1.1% |
| `delete_parent` | 3003.7 (3079.3 +- 181.0) | 3041.8 (3064.6 +- 138.6) | +1.3% |

The topics of `perf_tr_treedb` have no pkey2, so the guard itself does not
run; the save fetches the pkey2 list once and reuses it. `update_memory`
never reaches the changed code: it moved about +-5% between builds, and a
build with the new functions present and never called showed the same
swing, so that is code layout, not the change.

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
appends/s (+2.5%). So a difference of 2.5% between two links is not a
difference of the code.

To remove this effect, each module is padded to the same `.text` size and
linked 4 times, with the libraries moved by 0, 320, 704 and 1472 bytes. The
module of each release is compiled alone (`gcc -O2 -g -DNDEBUG`, the flags of
the module's build, with the headers of this release). A second object, made
with the assembler from `.section .text` / `.skip N, 0xcc`, is linked right
after it. N is the larger `.text` of the two modules rounded up to 64, minus
the `.text` of this module, plus the shift. Each binary is linked with
`gcc -O2 -g -DNDEBUG -static`, in the order of the benchmark's own
`link.txt`: its object, the module, the padding, then the libraries. So the
libraries start at the same address in both releases, and the shift moves
them by the same amount in both. 20 rounds of each of the 4 links, all the
binaries alternated (n = 80 for each column):

| Case | 7.25.4 | 7.25.5 | Change |
|------|--------|--------|--------|
| without an rt list | 220 120 +- 4 544 (220 070) | 221 588 +- 4 753 (220 424) | +0.7% |
| with an rt list | 166 498 +- 3 354 (165 816) | 167 381 +- 3 306 (166 189) | +0.5% |

Cycles in `tranger2_append_record()` (`rdtsc` probes in copies of the
module, median of 6 runs, both phases of the test): 7.25.4 14 580, 7.25.5
14 484. Where the append of 7.25.5 spends differently (cycles an append):

| Step | 7.25.4 | 7.25.5 |
|------|--------|--------|
| master check, topic | 168 | 203 |
| file id, flagged-file check | 373 | 450 |
| md2 open, `lseek()`, torn-tail check | 658 | 668 |
| find the cell, order marker | -- | 544 |
| cache update (7.25.4: with the find of the cell) | 1 617 | 934 |
| `json_dumps()` (the same code in both) | 5 784 | 5 791 |

The torn-tail check costs nothing when the md2 ends on a row boundary (one
`%`). The append of 7.25.5 looks up the key's cache once, and searches the
cache cell of its file once.

yev_loop (8 rounds):

| Benchmark | 7.25.4 | 7.25.5 | Change |
|------|--------|--------|--------|
| `perf_yev_ping_pong` (K msg/s) | 152.2 +- 3.3 (153.7) | 150.5 +- 2.9 (149.8) | -1.1% |
| `perf_tcp_test4` (ops/s) | 38 706 +- 685 (38 841) | 39 344 +- 771 (39 831) | +1.6% |
| `perf_tcp_test5` (ops/s) | 30 421 +- 641 (30 386) | 30 181 +- 597 (30 323) | -0.8% |

The last fixes of yev_loop (a take-back that wakes the loop, a stop without
memory that gives up nothing: commit 8702a472e), as an A/B of that change
alone, 6 rounds: `perf_yev_ping_pong` 142.2 +- 2.2 -> 141.6 +- 2.5 K msg/s
(-0.4%), noise.

rotatory (ns for one record of 300 bytes written with two `rotatory_write()`
calls, as the agent audit writes it; 300 000 records a run, 8 rounds;
`perf_rotatory`, case `audit_record`): 7.25.4
6 194 +- 135 (6 140), 7.25.5 554 +- 10 (555). The agent audit flushes each
record since 7.25.5 (`audit_record_flush`, a busier run): 7.25.4 7 623 +- 1 105
(7 252), 7.25.5 1 475 +- 491 (1 309).

The last fixes of rotatory (the newfile callback for a new file only, the
keep_all retry of a failed rename, a fixed name never emptied: commit
c72442d56), as an A/B of that change alone, 12 alternated rounds (ns a
record, mean +- standard deviation; another day, so the absolute figures
differ from the ones above):

| Case | before | after | Change |
|------|--------|-------|--------|
| `audit_record` | 591.5 +- 22.3 | 597.8 +- 21.3 | +1.1% |
| `audit_record_retention` | 576.9 +- 19.8 | 590.3 +- 18.8 | +2.3% |
| `audit_record_flush` | 1 336.8 +- 35.0 | 1 342.4 +- 45.3 | +0.4% |
| `log_record` | 401.9 +- 11.0 | 403.6 +- 13.5 | +0.4% |

Every difference is within one standard deviation; the only change on the
path of a record is one test of a bool. The flush of each record costs
0.7-0.9 us (`audit_record_flush` against `audit_record`).

The rotatory fixes after them (the newfile callback kept when a new file
fails to open, no size rotation at a new name, the keep_all retry on the
monotonic clock: commit 4994e4434), 10 alternated rounds, only `rotatory.c`
swapped (ns a record, mean +- standard deviation (median)):

| Case | before | after |
|------|--------|-------|
| `audit_record` | 706 +- 138 (670) | 696 +- 163 (640) |
| `audit_record_retention` | 670 +- 98 (624) | 657 +- 71 (630) |
| `audit_record_flush` | 1 502 +- 92 (1 476) | 1 589 +- 226 (1 474) |
| `log_record` | 452 +- 47 (432) | 454 +- 47 (438) |

The medians do not move. The agent audit of a JSON text inside a JSON text
(commit 452e83c00), the record built and serialized (us a record, 10
alternated rounds, mean +- standard deviation):

| Record | before | after | Change |
|------|--------|-------|--------|
| `list-yunos` | 1.631 +- 0.043 | 1.643 +- 0.056 | +0.7% |
| `run-yuno` | 5.335 +- 0.206 | 5.151 +- 0.048 | -3.4% |
| `update-node`, a kw with a JSON text | 6.309 +- 0.124 | 6.331 +- 0.205 | +0.3% |
| `update-node`, a JSON text holding an escaped JSON text | 6.318 +- 0.132 | 6.540 +- 0.144 | +3.5% |

Only a record whose kw holds an escaped JSON text pays: +0.22 us, one decode
and scan of the escaped run.

The scan that finds an escaped JSON text whatever quotes come before it, and
reads a key by its shape when no quoted run holds it whole (commit
7aabcab4b), the record built and serialized (us a record, 6 alternated
runs, mean; another day, so the absolute figures differ from the ones
above):

| Record | before | after | Change |
|------|--------|-------|--------|
| `list-yunos` | 1.60 | 1.54 | -3.8% |
| `run-yuno` | 6.83 | 6.83 | 0.0% |
| `update-node`, a kw with a JSON text | 8.11 | 8.18 | +0.9% |
| `update-node`, a JSON text holding an escaped JSON text | 8.28 | 8.41 | +1.6% |

`exit_on_fail` for `rotatory_open()` only, a later open that fails printed
and tried again (commit 93d347d09), 8 alternated rounds, only `rotatory.c`
swapped (ns a record, mean):

| Case | before | after | Change |
|------|--------|-------|--------|
| `audit_record` | 553 | 547 | -1.1% |
| `audit_record_retention` | 547 | 545 | -0.4% |
| `audit_record_flush` | 1 280 | 1 265 | -1.2% |
| `log_record` | 386 | 387 | +0.3% |

Only the failure paths of an open changed.

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

The probes cover only the steps listed. The benchmark says more: +24.3 ms an
open for `seed` and +27.5 ms for `newer_literal` ((10.84 - 9.87) / 40,
(13.89 - 12.79) / 40, against the probes' 16.5 and 21.9), so ~6-8 ms an open
is spent outside those steps.

The last schema fixes (a literal with other content under the file's
`schema_version` said at every open, and the move of a rowid-keyed
projection before the upgrade record: commit 2f692ac1d) cost `same_literal`
one `schema_diff()` and one more lookup of `treedbs`. A/B of that change
alone, 8 alternated rounds, ms an open, mean +- standard deviation (the
"before" is the 7.25.5 column above: 0.613 s / 40 = 15.3 ms):

| Case | before | after | Change |
|------|--------|-------|--------|
| `seed` | 282.5 +- 15.6 | 283.4 +- 14.0 | +0.3% |
| `newer_literal` | 345.1 +- 20.1 | 348.5 +- 15.8 | +1.0% |
| `same_literal` | 15.33 +- 1.46 | 16.20 +- 0.66 | +5.7% |

`same_literal` pays ~0.9 ms an open, the price of those two correctness fixes:
~0.648 s for 40 opens, still ~4% below 7.25.4's 0.672 s (the two figures come
from different runs). The "before" binary was linked against the installed
libraries as they were before the build of the change.

The schema fixes after them (the order of the file in a save, the positions a
save writes, a topic raised past what runs, the imposed tie said: commit
dd48e5e57), both `c_treedb.c` and `c_node.c` swapped, 8 alternated rounds on
a machine busy with other builds (ms for 40 opens, mean +- standard
deviation; another day, so the absolute figures differ from the ones above):

| Case | before | after | Change |
|------|--------|-------|--------|
| `seed` | 2786 +- 270 | 2913 +- 286 | +4.6% |
| `newer_literal` | 3012 +- 552 | 2836 +- 252 | -5.8% |
| `same_literal` | 835 +- 135 | 857 +- 140 | +2.6% |

Every difference is inside the noise of that run (a spread of 9-18%); it is
to be measured again on a quiet machine. The only new work on the
`same_literal` path is one `topic_var.json` read per topic (and its
`topic_cols.json` for a topic that runs ahead of its file); the seed and
newer-literal paths do nothing new.

yev_loop, the `src_url` of another family (commit bbd2c9ad7), 6 rounds:
`perf_yev_ping_pong` 148.7 +- 1.9 -> 148.4 +- 1.9 K msg/s (-0.2%), noise.
C_UDP_S has no benchmark in the tree. The read that takes a new gbuffer when
the host keeps the received one (commit 62fa7dbc4) was measured with a
scratch copy of the echo server of `tests/c/c_udp_s_echo` driven by a Python
client: 64-byte datagrams, 20 000 round trips a round, 8 alternated rounds,
11.88 +- 0.77 -> 11.59 +- 0.35 us a round trip (-2.4%), noise. When nobody
keeps the gbuffer, the common path pays one refcount test.

ctest timing trend (`build/*.txt`, 30 runs of `yunetas test` from 2026-09-23
to 2026-09-25): of the tests whose code did not change after 7.25.4, only
`test_treedb_schema_fidelity` moved more than 10%, from 1.14-1.26 s (the 4
runs before the change) to 1.55-1.81 s (the 26 after it; +36% .. +44%). It
opens four treedbs in four new stores, and its fsyncs (88 in the current
build) take ~0.56 s: the price of the durable topic files and of the
projection record. The `c_tcp` and `c_tcp2` tests wait on whole seconds of
their retry timers and moved by whole seconds in every period
(`c_tcp/test2` 9-15 s, `c_tcp/test3` 2-4 s): noise. `test_c_treedb_system_schema`,
`test_c_treedb_literal_wins` (2.3 s -> 138 s: a kill at every write of a
projection) and `test_tr_treedb_files` gained cases, so their times cannot be
compared. `timeranger2/test_topic_pkey_integer` stays at 1.84-2.12 s.

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
