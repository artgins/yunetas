# Performance Benchmarks (C)

Benchmark programs for measuring throughput and latency of Yuneta's core communication and I/O subsystems.

All benchmarks run as self-contained programs. The network ones start an internal server and client, echo messages back and forth, and report throughput metrics; the persistence ones (`perf_timeranger2`, `perf_tr_treedb`, `perf_c_treedb`) build a store, measure, print one line of JSON per result, and remove the store; `perf_rotatory` does the same with the log files. Each benchmark is also registered as a `ctest` target, so `yunetas test` runs them alongside the rest of the test suite (the persistence ones with smaller sizes: ctest checks that they build and run; their reference figures come from a run with the default sizes). Each one is `RUN_SERIAL`: under `ctest -j` it runs alone, so its figures are not skewed by the tests running at once, nor theirs by it. A new benchmark declares it too, and follows the rules of `tests/c/README.md` (its own ports and directory, archives linked by full path).

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

- **Timeranger2 DB:** `~/tests_yuneta/perf_yev_ping_pong2/topic_pkey_integer_ping_pong`
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

### perf_timeranger2 -- timeranger2 open, topic create, tm query, iterator reads

**Binary:** `perf_timeranger2`

timeranger2 alone: the open of a store of 20 000 md2 files as a master and as a replica, the create of 10 topics and 10 `topic_version` changes, a tm query of one minute, and the read path of an iterator (open, pages of 1 and 100 rows, unfiltered and filtered) as a master and as a replica. See [`perf_timeranger2/README.md`](perf_timeranger2/README.md).

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

The unlink and the delete that clear the parent's ref in every instance of
the child key, the create that takes the primary's slot, and the gc that
keeps an instance's asset (commit fdf138144), as an A/B of that change alone
(only `tr_treedb.o` and `kwid.o` differ; 12 alternated rounds, `taskset`
to one CPU, CPU us per operation, mean +- standard deviation (median); the
change is the mean of the 12 paired ratios after/before +- its standard
error):

| Case | before | after | Paired change |
|------|--------|-------|---------------|
| `update_memory` (no save) | 2.820 +- 0.021 (2.826) | 2.753 +- 0.052 (2.753) | -2.4% +- 0.6% |
| `update_saved` | 10.61 +- 0.14 (10.55) | 10.21 +- 0.12 (10.16) | -3.8% +- 0.4% |
| `link_unlink` | 10.92 +- 0.24 (10.84) | 10.95 +- 0.47 (10.78) | +0.3% +- 0.7% |
| `create_link_half` | 70.56 +- 1.39 (71.00) | 71.42 +- 2.82 (70.48) | +1.3% +- 1.4% |
| `reopen` | 393.3 +- 7.3 (394.7) | 390.2 +- 7.2 (389.6) | -0.8% +- 0.7% |
| `delete_force` | 68.5 +- 4.9 (66.8) | 69.4 +- 1.4 (69.2) | +1.7% +- 2.0% |
| `delete_parent` | 3040 +- 114 (3003) | 2944 +- 65 (2923) | -3.0% +- 1.2% |

No loss: the moves are -3.8% .. +1.7%, and the two outside their error are
faster (`update_saved`, and `update_memory`, which does not reach the
changed code at all), the layout effect of a relink described above. The
topics of `perf_tr_treedb` have no pkey2, and the new scans of the instances
run only for a child topic that has one.

The newest record of each key kept on the instance that wrote it (a forced
delete, a refused delete or a failed unlink taken back, a snap shot: commit
41b6597c8), as an A/B of that change alone (only `tr_treedb.o` differs; 12
alternated rounds, `taskset` to one CPU, CPU us per operation, mean +-
standard deviation):

| Case | before | after | Change |
|------|--------|-------|--------|
| `update_saved` | 10.955 +- 0.11 | 10.947 +- 0.53 | -0.1% |
| `delete_force` | 73.07 +- 4.1 | 74.53 +- 5.4 | +2.0% |
| `delete_parent` | 3308 +- 409 | 3210 +- 134 | -3.0% |

Every move is inside the spread of its case: noise. The benchmark's topics
have no pkey2, so there the new code is one lookup per child.

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
~0.75 us: `audit_record_flush` against `audit_record` of the same run,
1 342.4 - 597.8 = 745 ns (and 1 336.8 - 591.5 = 745 ns in the "before"
column; the `exit_on_fail` run below gives 727 and 718 ns).

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

The size limit compared in bytes instead of whole megabytes (commit
ee3cd4081), 8 alternated rounds, only `rotatory.c` swapped (ns a record,
mean +- standard deviation (median)):

| Case | before | after | Change |
|------|--------|-------|--------|
| `audit_record` | 555 +- 9 (554) | 574 +- 26 (559) | +3.4% |
| `audit_record_retention` | 564 +- 25 (557) | 565 +- 12 (561) | +0.1% |
| `audit_record_flush` | 1 310 +- 18 (1 305) | 1 300 +- 10 (1 297) | -0.8% |
| `log_record` | 391 +- 12 (386) | 388 +- 4 (388) | -0.8% |

Noise: the `audit_record` mean is pulled by three rounds of the "after"
binary at 602-608 ns, and its median moves +0.9%. The size check is one
comparison per record in both.

The audit fixes that followed (an escaped key read as its whole string, a
secret value taken as its whole shell word, a JWT before a `.`: commit
d85dbf66d), the record built and serialized (us a record, 10 alternated
rounds, mean (median)):

| Record | before | after | Change |
|------|--------|-------|--------|
| `list-yunos` | 1.59 (1.58) | 1.70 (1.56) | +6.9% (median -1.3%) |
| `run-yuno` | 6.88 (6.82) | 6.97 (6.90) | +1.4% |
| `update-node`, a kw with a JSON text | 8.22 (8.20) | 8.31 (8.20) | +1.1% |
| `update-node`, a JSON text holding an escaped JSON text | 8.49 (8.50) | 8.60 (8.63) | +1.4% |

The `list-yunos` mean is one outlier round (2.16 us); its median does not
move. The rest is +1.1% .. +1.4%, inside the spread of a run.

The names judged with their JSON escapes decoded, and a write-attr looked for
in every string of the kw (commit b3f69daf5), 8 alternated rounds (us a
record, mean (median)):

| Record | before | after | Change |
|------|--------|-------|--------|
| `list-yunos` | 1.51 (1.50) | 1.55 (1.55) | +2.4% |
| `run-yuno` | 6.44 (6.42) | 6.57 (6.59) | +2.1% |
| `update-node`, a kw with a JSON text | 7.74 (7.75) | 7.98 (7.97) | +3.1% |
| `update-node`, a JSON text holding an escaped JSON text | 8.05 (8.08) | 8.29 (8.30) | +3.0% |

Every record pays +2-3% (0.04-0.24 us): each string of the kw is now
checked for a write-attr, and a name that holds `\u` is decoded once per
level.

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

yev_loop, the close of an fd that takes back the submissions of other
events on it, and the zero-copy notification waited for at the end of the
loop (commit 354e818e6), 8 alternated rounds: `perf_yev_ping_pong` 137.8 +-
5.0 -> 140.5 +- 4.6 K msg/s (+2.0%), noise. The scan runs only when an fd is
closed with something pending on it.

yev_loop, the entry of the ring cleared when it is handed out (its fd and its
owner), and the cancel of a running stop prepared before the fd closes
(commit 3dc800efa), 6 alternated rounds, K msg/s of `perf_yev_ping_pong`:
before 153.9 / 142.0 / 137.3 / 148.2 / 147.2 / 152.7, after 150.3 / 123.0 /
147.5 / 147.8 / 147.9 / 152.9. Medians 147.7 -> 147.9 (+0.1%): no change;
the 123.0 is one round alone.

Publish (`gobj_publish_event()`), a subscription with `__local__` or
`__global__` given its own kw (commits 42a6818b6, then b307eef3f). There is
no publish benchmark in the tree; a scratch one (`bench_publish.c`, not in
the tree) publishes one event to N subscribers of one gobj, a kw of ~250
bytes and 18 values, or of ~20 KB; only `gobj.o` differs between the
binaries (`-O2`), all alternated (us a publish, mean; 8 rounds of each at
250 B, 16 at 20 KB). "7.25.4" is the shared kw of 7.25.4, "deep" is a
`kw_duplicate()` per such subscription (42a6818b6), "twin" is the
`kw_twin()` that the release ships (its own top level, the values and
gbuffers shared):

| kw | `__global__` | subscribers | 7.25.4 | deep | twin | twin against 7.25.4 |
|----|----|----|----|----|----|----|
| 250 B | no | 1 | 1.78 | 1.78 | 1.78 | -0.1% |
| 250 B | no | 10 | 3.20 | 3.28 | 3.21 | +0.3% |
| 250 B | no | 100 | 19.86 | 19.19 | 18.86 | -5.0% (sd 3.2 us, noise) |
| 250 B | yes | 1 | 1.95 | 3.88 | 2.08 | +6.6% |
| 250 B | yes | 10 | 3.71 | 21.51 | 5.93 | +59.9% |
| 250 B | yes | 100 | 21.76 | 200.28 | 44.94 | +106.5% |
| 20 KB | no | 1 | 63.03 | 62.56 | 62.76 | -0.4% |
| 20 KB | no | 10 | 64.98 | 64.36 | 64.22 | -1.2% |
| 20 KB | no | 100 | 79.59 | 79.36 | 81.28 | +2.1% (sd 12.4 us, noise) |
| 20 KB | yes | 1 | 64.37 | 118.42 | 63.88 | -0.7% |
| 20 KB | yes | 10 | 65.65 | 620.01 | 67.54 | +2.9% |
| 20 KB | yes | 100 | 82.44 | 5 554.46 | 104.93 | +27.3% |

A subscriber without `__local__` / `__global__` still shares the publisher's
kw: nothing moves beyond the noise. A twinned delivery costs ~0.23 us,
whatever the size of the event ((44.94 - 21.76) / 100 at 250 B, (104.93 -
82.44) / 100 at 20 KB); the deep copy cost ~1.8 us at 250 B and ~55 us at
20 KB. Building the 20 KB kw takes ~60 us of each 20 KB publish, so the
percentages of that half understate the change. Every C_IEVENT_SRV
subscription carries `__global__` (the gate's back-metadata), so a remote
subscriber pays the twin, beside the serialization of the kw it already
paid. This is the price of the fix of a peer's subscription that changed the
event of every later subscriber (see the CHANGELOG, "Security").

Subscribe (`gobj_subscribe_event()` / `gobj_unsubscribe_event()`), the kw
matched as it is stored (a repeated `__own_event__` / `__rename_event_name__`
subscription replaces the one there: commit b8aea552b). A scratch benchmark
(`bench_subscribe.c`, not in the tree) makes N subscriptions of one gobj and
withdraws them; only `gobj.o` differs, 4 alternated rounds, us an operation,
the median of the mean of a subscribe and an unsubscribe:

| Subscriptions | `__config__` | before | after | Change |
|----|----|----|----|----|
| 100 | no | 8.63 | 8.47 | -1.9% (one cold round of 26 us in "after") |
| 100 | yes | 11.24 | 10.69 | -4.8% |
| 2000 | no | 167.68 | 158.41 | -5.5% |
| 2000 | yes | 225.98 | 218.58 | -3.3% |

The publish of the same run (`bench_publish.c`, above) moves -3.4% .. +2.7%
(medians), noise. C_IEVENT_SRV's own subscribe adds one scan of the
channel's subscriptions (at most `max_subscriptions`), and it stores a
smaller back-metadata, which every event of the subscription copies: not
measured.

ctest timing trend (`build/*.txt`, 52 runs of `yunetas test` from 2026-09-23
10:20 to 2026-09-25 12:26): of the tests whose code did not change after
7.25.4, only `test_treedb_schema_fidelity` moved more than 10%, from
1.14-1.26 s (the 4 runs before the change, median 1.19 s) to 1.55-1.81 s (the
48 after it, median 1.66 s; +36% .. +44% minimum against minimum and maximum
against maximum, +39% median against median). It
opens four treedbs in four new stores, and its fsyncs (88 in the current
build) take ~0.56 s: the price of the durable topic files and of the
projection record. The `c_tcp`, `c_tcp2` and `c_tcps` tests wait on whole seconds of
their retry timers and moved by whole seconds in every period
(`c_tcp/test2` 9-15 s, `c_tcp/test3` 2-4 s, `c_tcps/test1` 12-14 s): noise.
`test_c_treedb_system_schema`, `test_c_treedb_literal_wins` (2.3 s -> ~225 s,
218-235 s in the runs since 2026-09-24 18:59: a kill at every write of a
projection), `test_tr_treedb_delete_instance` and `test_tr_treedb_files`
gained cases, so their times cannot be compared. `timeranger2/test_topic_pkey_integer` stays at 1.84-2.12 s.

### Sep-2026: 7.25.20 against 7.25.5 (RelWithDebInfo, `CONFIG_DEBUG_TRACK_MEMORY` on)

Each release built whole from its own tree (a git worktree of the 7.25.5 tag,
the same `.config`, compiler and `outputs_ext`), the two binaries of each
benchmark run alternately, the order flipped every round, with a `sync` and a
3 s pause before each run: 8 rounds, 24 for the three timeranger2 ctest
binaries. Mean +- standard deviation. No module was padded or relinked, so a
change under ~2.5% on an append is not told apart from code placement. The
report: [`performance/reports/7.25.20.html`](../reports/7.25.20.html).

| Case | 7.25.5 | 7.25.20 | Change |
|------|--------|---------|--------|
| `perf_c_treedb` `same_literal` (s) | 0.667 +- 0.051 | 0.318 +- 0.009 | x2.1 faster |
| `perf_c_treedb` `seed` (s) | 11.09 +- 0.23 | 10.19 +- 0.40 | -8.1% |
| `perf_c_treedb` `newer_literal` (s) | 13.56 +- 0.56 | 12.85 +- 0.71 | -5.3% (noise) |
| `perf_tr_treedb` `update_memory` (us, CPU) | 2.762 +- 0.051 | 2.316 +- 0.037 | -16.2% |
| `perf_tr_treedb` `update_saved` | 10.25 +- 0.26 | 7.66 +- 0.16 | -25.2% |
| `perf_tr_treedb` `link_unlink` | 10.85 +- 0.23 | 8.65 +- 0.18 | -20.3% |
| `perf_tr_treedb` `create_link_half` | 69.7 +- 1.0 | 64.9 +- 1.3 | -7.0% |
| `perf_tr_treedb` `delete_parent` | 2996 +- 68 | 2478 +- 45 | -17.3% |
| `perf_tr_treedb` `reopen`, `delete_force` | 392.0, 69.4 | 386.0, 68.5 | noise |
| `test_topic_pkey_integer`, appends/s | 226,148 +- 5,101 | 226,720 +- 4,468 | +0.3% |
| the same, with an rt list | 169,624 +- 3,376 | 191,284 +- 4,132 | +12.8% |
| `perf_timeranger2` `build_appends` (ms) | 1717.6 +- 26.0 | 1750.3 +- 27.1 | +1.9% (noise) |
| `perf_timeranger2` `tm_build_appends` (ms) | 1632.1 +- 38.6 | 1690.7 +- 40.2 | +3.6% (see below) |
| `perf_timeranger2` `open_master`, `open_replica` (ms) | 78.6, 103.7 | 78.2, 104.0 | noise |
| `perf_timeranger2` `tm_query_migrated`, `unmigrated` (ms) | 7.35, 390.1 | 7.35, 389.7 | noise |
| reads, iterator / pages (records/s) | 189,582 / 169,498 | 188,895 / 168,796 | noise |
| `perf_rotatory` audit / flushed / log (ns) | 569 / 1244 / 374 | 572 / 1244 / 375 | noise |
| `perf_yev_ping_pong` / `2` (K msg/s) | 150.2 / 85.1 | 150.1 / 84.3 | noise |
| `perf_tcp_test4` / `5` (round trips/s) | 38,151 / 29,614 | 38,191 / 30,073 | noise |
| `perf_tcps_test4` / `5` (round trips/s) | 27,868 / 23,630 | 27,849 / 23,271 | noise |
| `perf_auth_bff` (logins/s) | 8,448 | 8,347 | noise |

Every gain has one cause, the `SWITCHS` fix of 7.25.7 (`3e8ff0508`): the macro
compiled `regcomp(".*")` on entry and freed it only at `SWITCHS_END` (it leaked
on every `return` from a case). `tr_treedb.c` switches on the type of every
column, so each treedb write paid for it; `tr_treedb.c` and `c_treedb.c` did
not change between the two releases. The rt-list gain is the benchmark's own
callback, which switches on the key once per record.

`tm_build_appends` +3.6% (t = 3.0) runs append code that is the same in both
releases (`timeranger2.c` changed only in its rt-disk rescan): code
placement, as far as this method can tell; `test_topic_pkey_integer`, 24 rounds,
moved +0.3%.

### Oct-2026: 7.27.7 against 7.27.5 (RelWithDebInfo, `CONFIG_DEBUG_TRACK_MEMORY` on)

Written on 2026-10-22, before the tag. 7.27.7 is the main tree at `63d12373d`
(its C code is final; YUNETA_VERSION still says 7.27.6). It is measured
against 7.27.5, the last release with a study; 7.27.6 changed `.c` files and
shipped none, so it runs in the same rotation and every move is placed in its
release. The three built from git worktrees (same `.config` and compiler,
`outputs_ext` shared: linux-ext-libs 1.24 at all three, OpenSSL 3.6.5), the
`perf_timeranger2` of the main tree (with its `read` phase) compiled into
all three. The three binaries of each benchmark run in rotation, the order
turned every round, a `sync` and a 3 s pause before each run (1 s for the
ctest binaries and the read phase): 8 rounds; 24 for `perf_tr_treedb`,
`perf_tcp_test4`, `perf_tcps_test4` and `perf_yev_ping_pong2`; 48 for
`perf_timeranger2 read`; `perf_rotatory` 16, its 8th round left out (the
machine slowed the three builds alike for that minute: a log line 574 ns
against 368 ns). Mean +- standard deviation; treedb figures are CPU time;
|t| < 3 is noise. The report:
[`performance/reports/7.27.7.html`](../reports/7.27.7.html).

| Case | 7.27.5 | 7.27.7 | Change |
|------|--------|--------|--------|
| tr2.append_rate (appends/s) | 231,340 +- 3,507 | 232,605 +- 3,117 | +0.6% (t = +0.8) |
| tr2.append_rate_rt (appends/s) | 196,704 +- 3,402 | 198,131 +- 2,979 | +0.7% (t = +0.9) |
| tr2.build_appends (ms) | 1,574 +- 12.86 | 1,563 +- 16.82 | -0.7% (t = -1.5) |
| tr2.tm_build_appends (ms) | 1,642 +- 31.98 | 1,661 +- 27.54 | +1.1% (t = +1.3) |
| tr2.read_rate_iterator (records/s) | 219,256 +- 3,828 | 221,403 +- 2,609 | +1.0% (t = +1.3) |
| tr2.read_rate_pages (records/s) | 195,565 +- 2,980 | 196,348 +- 4,674 | +0.4% (t = +0.4) |
| tr2.open_master (ms) | 78.88 +- 0.886 | 78.78 +- 0.518 | -0.1% (t = -0.3) |
| tr2.tm_query (ms) | 12.89 +- 0.345 | 12.86 +- 0.223 | -0.3% (t = -0.2) |
| tr2.open_replica (ms) | 92.40 +- 0.473 | 93.48 +- 1.720 | +1.2% (t = +1.7) |
| tr2.create_topic (ms) | 145.3 +- 16.55 | 151.3 +- 10.67 | +4.1% (t = +0.9) |
| tr2.topic_version_change (ms) | 253.5 +- 19.35 | 266.4 +- 25.38 | +5.1% (t = +1.1) |
| treedb.update_memory (us) | 2.331 +- 0.065 | 2.359 +- 0.064 | +1.2% (t = +1.5) |
| treedb.update_saved (us) | 7.528 +- 0.135 | 7.590 +- 0.267 | +0.8% (t = +1.0) |
| treedb.link_unlink (us) | 7.352 +- 0.085 | 7.380 +- 0.146 | +0.4% (t = +0.8) |
| treedb.create_link_half (us) | 62.71 +- 1.423 | 63.19 +- 1.570 | +0.8% (t = +1.1) |
| treedb.reopen (us) | 281.8 +- 6.392 | 281.2 +- 5.863 | -0.2% (t = -0.3) |
| treedb.delete_force (us) | 43.63 +- 0.926 | 52.84 +- 0.600 | +21.1% (t = +40.9) **price** |
| treedb.delete_parent (us) | 2,251 +- 48.51 | 2,303 +- 78.84 | +2.3% (t = +2.7) |
| ctreedb.same_literal_release (s) | 0.357 +- 0.069 | 0.331 +- 0.005 | -7.4% (t = -1.1) |
| ctreedb.seed (s) | 11.16 +- 0.492 | 11.24 +- 0.632 | +0.7% (t = +0.3) |
| ctreedb.newer_literal (s) | 13.53 +- 0.803 | 13.21 +- 0.686 | -2.3% (t = -0.8) |
| rotatory.audit_record (ns) | 567.9 +- 22.94 | 572.3 +- 22.67 | +0.8% (t = +0.5) |
| rotatory.audit_record_flush (ns) | 1,251 +- 15.92 | 1,248 +- 15.66 | -0.2% (t = -0.4) |
| rotatory.log_record (ns) | 375.0 +- 6.218 | 375.3 +- 6.337 | +0.1% (t = +0.1) |
| net.yev_ping_pong (K msg/s) | 148.4 +- 1.173 | 148.3 +- 1.032 | -0.0% (t = -0.1) |
| net.yev_ping_pong_persist (K msg/s) | 80.71 +- 7.434 | 82.69 +- 6.733 | +2.5% (t = +1.0) |
| net.tcp_echo (round trips/s) | 38,175 +- 1,259 | 39,107 +- 1,574 | +2.4% (t = +2.3) |
| net.tcp_echo_persist (round trips/s) | 30,909 +- 431.5 | 30,941 +- 363.2 | +0.1% (t = +0.2) |
| net.tls_echo (round trips/s) | 28,640 +- 1,134 | 28,694 +- 2,771 | +0.2% (t = +0.1) |
| net.tls_echo_persist (round trips/s) | 24,707 +- 495.9 | 24,266 +- 351.0 | -1.8% (t = -2.0) |
| net.auth_bff_login (logins/s) | 8,606 +- 90.14 | 8,672 +- 117.0 | +0.8% (t = +1.3) |

**The read path** (`perf_timeranger2 read`, us per operation, 48 rounds):

| Case | 7.27.5 | 7.27.7 | Change |
|------|--------|--------|--------|
| `read_page1_replica` | 6.272 +- 0.318 | 6.309 +- 0.273 | +0.6% (t = +0.6) |
| `read_page1_filtered_replica` | 5.045 +- 0.313 | 4.998 +- 0.242 | -0.9% (t = -0.8) |
| `read_open_replica` | 17.91 +- 1.262 | 18.10 +- 1.520 | +1.1% (t = +0.7) |
| `read_open_filtered_replica` | 13.24 +- 1.491 | 13.49 +- 1.688 | +1.9% (t = +0.8) |
| `read_page100_replica` | 276.6 +- 27.31 | 271.5 +- 10.74 | -1.8% (t = -1.2) |
| `read_page1_master` | 5.305 +- 0.200 | 5.337 +- 0.260 | +0.6% (t = +0.7) |
| `read_page1_filtered_master` | 4.163 +- 0.219 | 4.197 +- 0.232 | +0.8% (t = +0.8) |
| `read_open_master` | 16.85 +- 1.659 | 16.62 +- 0.971 | -1.3% (t = -0.8) |
| `read_open_filtered_master` | 12.13 +- 1.241 | 12.09 +- 0.692 | -0.3% (t = -0.2) |
| `read_page100_master` | 272.1 +- 16.54 | 268.7 +- 10.07 | -1.2% (t = -1.2) |

One price, `treedb.delete_force` (+21.1%, 43.6 -> 52.8 us of CPU), and it
appeared in **7.27.6, commit `503c23efb`** (the only C change of that
release): every key a delete removes is first moved out of `keys/` whole, by
a `rename()`, then removed, so a delete that fails half way leaves the key as
it was instead of part of its files (a key read again from part of its files
renumbered the rows after the hole). Placed by the rotation and split with an
attribution build (7.27.7 with `process_start_time()` read once per process
in `remove_key_directory()`, never shipped), CPU us per delete, 24 rounds
(16 for the attribution build):

| 7.27.5 | 7.27.6 | 7.27.7 | 7.27.7, start time read once |
|--------|--------|--------|------------------------------|
| 43.63 +- 0.93 | 52.69 +- 0.70 (+20.8%, t = 38) | 52.84 +- 0.60 (+21.1%, t = 41; +0.3% on 7.27.6, t = 0.8) | 47.75 +- 0.65 (+9.4%, t = 17) |

About 5 us of the 9.2 us is `process_start_time()`, a read of
`/proc/self/stat` made at every delete to name the moved directory; about
4 us is the `rename()`. The start time of a process does not change, so that
half can be paid once per process. Everything else moved inside its noise:
the fixes of V38-V40 (iterator bounds on a replica, the purge and link order,
the `.purged` retry, the owed reads of an rt_disk follower, the parts of a
multi-key `C_TRANGER` iterator) cost no measurable time on the read path of
the master or of a replica. `C_TRANGER` itself is not in a benchmark: its
multi-key part bounds run only on a renumbered replica. Stripped sizes: 0 to
+0.08% against 7.27.5 (at most 8 KiB).

### Oct-2026: 7.27.5 against 7.27.0 (RelWithDebInfo, `CONFIG_DEBUG_TRACK_MEMORY` on)

Written on 2026-10-22, after the release, against 7.27.0: the last release
with a study. It replaces the study of the day before, made against 7.27.4,
a release nobody had measured; this one covers 7.27.1 to 7.27.5. Each release
built from a git worktree of its tag (same `.config` and compiler), the
`perf_timeranger2` of the main tree (with its `read` phase) compiled into
both. `outputs_ext` shared: linux-ext-libs 1.24 (OpenSSL 3.6.5) on both
sides, so the library change of 7.27.1 is not in these figures (it was
measured on its own, section *7.27.1 against 7.27.0* below). The two binaries
of each benchmark run alternately, a `sync` and a 3 s pause before each run
(1 s for the ctest binaries and the read phase): 16 rounds; 24 for
`perf_timeranger2` and `perf_tr_treedb`, with 7.27.4 in a rotation of three;
48 for `perf_timeranger2 read`; `perf_rotatory` 15 (its 9th round left out:
the machine slowed both builds alike for that minute). Mean +- standard
deviation; treedb figures are CPU time; |t| < 3 is noise. The report:
[`performance/reports/7.27.5.html`](../reports/7.27.5.html).

| Case | 7.27.0 | 7.27.5 | Change |
|------|--------|--------|--------|
| tr2.append_rate (appends/s) | 223,947 +- 5,866 | 226,311 +- 8,668 | +1.1% (t = +0.9) |
| tr2.append_rate_rt (appends/s) | 189,169 +- 5,660 | 193,185 +- 6,566 | +2.1% (t = +1.9) |
| tr2.build_appends (ms) | 1,595 +- 42.94 | 1,624 +- 57.96 | +1.8% (t = +1.9) |
| tr2.tm_build_appends (ms) | 1,688 +- 38.31 | 1,688 +- 40.34 | +0.0% (t = +0.0) |
| tr2.read_rate_iterator (records/s) | 215,304 +- 5,659 | 216,080 +- 8,155 | +0.4% (t = +0.3) |
| tr2.read_rate_pages (records/s) | 189,844 +- 5,666 | 193,624 +- 7,997 | +2.0% (t = +1.5) |
| tr2.open_master (ms) | 79.65 +- 2.161 | 81.14 +- 3.231 | +1.9% (t = +1.9) |
| tr2.tm_query (ms) | 12.70 +- 0.854 | 13.18 +- 0.405 | +3.8% (t = +2.5) |
| tr2.open_replica (ms) | 92.78 +- 2.201 | 94.36 +- 2.728 | +1.7% (t = +2.2) |
| tr2.create_topic (ms) | 143.4 +- 15.59 | 142.5 +- 14.30 | -0.7% (t = -0.2) |
| tr2.topic_version_change (ms) | 250.5 +- 28.68 | 254.2 +- 29.76 | +1.5% (t = +0.4) |
| treedb.update_memory (us) | 2.427 +- 0.055 | 2.364 +- 0.091 | -2.6% (t = -2.9) |
| treedb.update_saved (us) | 7.683 +- 0.218 | 7.620 +- 0.152 | -0.8% (t = -1.2) |
| treedb.link_unlink (us) | 7.554 +- 0.189 | 7.482 +- 0.172 | -0.9% (t = -1.4) |
| treedb.create_link_half (us) | 63.88 +- 2.104 | 64.24 +- 2.347 | +0.6% (t = +0.6) |
| treedb.reopen (us) | 289.2 +- 7.809 | 287.2 +- 8.236 | -0.7% (t = -0.9) |
| treedb.delete_force (us) | 42.87 +- 1.919 | 44.52 +- 2.647 | +3.9% (t = +2.5) **price** |
| treedb.delete_parent (us) | 2,327 +- 163.2 | 2,343 +- 190.1 | +0.7% (t = +0.3) |
| ctreedb.same_literal_release (s) | 0.374 +- 0.059 | 0.374 +- 0.062 | -0.0% (t = -0.0) |
| ctreedb.seed (s) | 10.81 +- 0.386 | 10.79 +- 0.517 | -0.1% (t = -0.1) |
| ctreedb.newer_literal (s) | 13.03 +- 0.597 | 13.00 +- 0.583 | -0.2% (t = -0.1) |
| rotatory.audit_record (ns) | 585.7 +- 36.27 | 573.0 +- 26.80 | -2.2% (t = -1.1) |
| rotatory.audit_record_flush (ns) | 1,265 +- 35.20 | 1,262 +- 41.69 | -0.3% (t = -0.3) |
| rotatory.log_record (ns) | 381.4 +- 8.056 | 382.6 +- 22.07 | +0.3% (t = +0.2) |
| net.yev_ping_pong (K msg/s) | 143.5 +- 7.723 | 143.1 +- 8.491 | -0.3% (t = -0.1) |
| net.yev_ping_pong_persist (K msg/s) | 85.01 +- 3.188 | 82.62 +- 3.126 | -2.8% (t = -2.1) |
| net.tcp_echo (round trips/s) | 38,258 +- 1,790 | 38,301 +- 1,353 | +0.1% (t = +0.1) |
| net.tcp_echo_persist (round trips/s) | 30,137 +- 986.7 | 30,131 +- 1,095 | -0.0% (t = -0.0) |
| net.tls_echo (round trips/s) | 29,284 +- 995.4 | 28,759 +- 1,278 | -1.8% (t = -1.3) |
| net.tls_echo_persist (round trips/s) | 23,942 +- 1,072 | 23,709 +- 1,116 | -1.0% (t = -0.6) |
| net.auth_bff_login (logins/s) | 8,329 +- 330.0 | 8,417 +- 292.3 | +1.1% (t = +0.8) |

**The read path** (`perf_timeranger2 read`, us per operation, 48 rounds):

| Case | 7.27.0 | 7.27.5 | Change |
|------|--------|--------|--------|
| `read_page1_replica` | 5.253 +- 0.208 | 6.196 +- 0.316 | +17.9% (t = +17.2) **price** |
| `read_page1_filtered_replica` | 4.076 +- 0.108 | 4.954 +- 0.110 | +21.5% (t = +39.6) **price** |
| `read_open_replica` | 16.54 +- 0.479 | 17.68 +- 0.685 | +6.8% (t = +9.4) **price** |
| `read_open_filtered_replica` | 11.77 +- 0.361 | 12.77 +- 0.386 | +8.5% (t = +13.1) **price** |
| `read_page100_replica` | 264.1 +- 5.833 | 268.1 +- 5.287 | +1.5% (t = +3.5) **price** |
| `read_page1_master` | 5.202 +- 0.137 | 5.210 +- 0.128 | +0.2% (t = +0.3) |
| `read_page1_filtered_master` | 4.074 +- 0.150 | 4.105 +- 0.124 | +0.8% (t = +1.1) |
| `read_open_master` | 16.67 +- 1.762 | 16.40 +- 0.820 | -1.6% (t = -1.0) |
| `read_open_filtered_master` | 11.74 +- 0.799 | 11.68 +- 0.471 | -0.5% (t = -0.5) |
| `read_page100_master` | 264.6 +- 7.431 | 267.6 +- 9.940 | +1.1% (t = +1.7) |

**Where each price appeared.** The `read` phase alone, 24 rounds, every tag
from 7.27.0 to 7.27.5 and two commits of 7.27.5 in one rotation (us per
operation):

| Case | 7.27.0 | 7.27.1 | 7.27.2 | 7.27.3 | 7.27.4 | `bd3fdce51` | `83a771089` | 7.27.5 |
|------|--------|--------|--------|--------|--------|-------------|-------------|--------|
| `read_page1_replica` | 5.126 | 5.119 | 5.163 | 5.216 | 5.118 | 5.194 | 6.029 | 6.101 |
| `read_page1_filtered_replica` | 3.980 | 3.984 | 3.983 | 3.996 | 3.983 | 4.017 | 4.793 | 4.890 |
| `read_open_replica` | 16.105 | 16.133 | 16.140 | 16.157 | 16.177 | 16.425 | 17.209 | 17.468 |
| `read_open_filtered_replica` | 11.492 | 11.440 | 11.545 | 11.695 | 11.470 | 11.693 | 12.612 | 12.735 |
| `read_page100_replica` | 260.989 | 260.278 | 264.587 | 260.559 | 261.702 | 266.418 | 267.026 | 265.866 |
| `read_page100_master` | 257.869 | 258.806 | 262.786 | 259.563 | 261.731 | 265.695 | 265.697 | 264.583 |

The price of a replica is `check_purge_before_read()` and **appeared in
7.27.5, commit `83a771089`** (*the followers hear a purge of old key
files*): 7.27.1 to 7.27.4 and `bd3fdce51` read at 7.27.0's speed. A replica
`stat()`s the first md2 file of the key before every iterator open and every
page, purged or not, to notice a purge of the master whose signal it did not
hear (an overflow of its watch, a follower down while the master purged, a
replica without a follower) before the read meets files that are gone: about
1 us per call, +18% on a page of 1 row, +22% on a filtered one, +7-9% on an
open, nothing on the master. It stays: skipping it while the replica seems to
have missed nothing leaves a window (a signal not yet read from the event
queue) in which a read opens a deleted file. A page of 100 rows (+1.5% on a
replica, t = 3.5; +1.1% on the master, t = 1.7) **appeared in 7.27.5, commit
`bd3fdce51`** (the first of retention), on the master as well: a page of an
unfiltered iterator counts its rows as positions among the live rows
(`g_rowid` = purged + position), so the base of the key is read for every
row. `treedb.delete_force` (+3.9%, t = 2.5) is 7.27.5 too: 7.27.4 42.0 us,
7.27.5 44.5 us (+6.1%, t = 4.5) -- a key delete looks for the key's base
(`.purged`, one `access()`) before it removes the directory, so a key with a
base is moved out of `keys/` whole (`remove_key_directory()`). Everything
else moved inside its noise, the gobj and TCP changes of 7.27.2 to 7.27.4
included. The first study (against 7.27.4) found the TCP and TLS echo and
the event loop echo with an append 2.0-2.5% slower where no code changed;
against 7.27.0 they are -2.8% to +0.1% (t = -2.1 to +0.1), inside their noise.

**The attribution build of the first study** (against 7.27.4;
`perf_timeranger2 read`: one key of 10 000 rows in 10 daily files; us per
operation). 24 rounds, four binaries in rotation, a
`sync` and a 1 s pause before each run: 7.27.4, 7.27.5, the main tree at
`24da06bc8` (after 7.27.6), and that tree with `check_purge_before_read()`
made to return at once (an attribution build, never shipped).

| Case | 7.27.4 | 7.27.5 | main (24da06bc8) | main, no check |
|------|--------|--------|------------------|----------------|
| `read_open_master` | 15.762 +- 0.672 | 16.091 +- 0.474 (+2.1%) | 15.956 +- 0.622 (+1.2%) | 15.970 +- 0.352 (+1.3%) |
| `read_open_filtered_master` | 11.279 +- 0.345 | 11.596 +- 0.414 (+2.8%) | 11.586 +- 0.303 (+2.7%) | 11.435 +- 0.519 (+1.4%) |
| `read_page1_master` | 5.108 +- 0.153 | 5.129 +- 0.100 (+0.4%) | 5.178 +- 0.287 (+1.4%) | 5.075 +- 0.095 (-0.6%) |
| `read_page1_filtered_master` | 3.967 +- 0.084 | 4.056 +- 0.123 (+2.2%) | 4.046 +- 0.124 (+2.0%) | 3.979 +- 0.066 (+0.3%) |
| `read_page100_master` | 260.547 +- 7.277 | 262.755 +- 7.135 (+0.8%) | 264.113 +- 5.839 (+1.4%) | 262.729 +- 7.005 (+0.8%) |
| `read_open_replica` | 16.054 +- 0.781 | 17.335 +- 0.408 (+8.0%) | 17.372 +- 0.735 (+8.2%) | 16.510 +- 0.586 (+2.8%) |
| `read_open_filtered_replica` | 11.465 +- 0.319 | 12.685 +- 0.302 (+10.6%) | 12.787 +- 0.365 (+11.5%) | 11.700 +- 0.362 (+2.1%) |
| `read_page1_replica` | 5.114 +- 0.088 | 6.078 +- 0.137 (+18.9%) | 6.009 +- 0.122 (+17.5%) | 5.114 +- 0.113 (-0.0%) |
| `read_page1_filtered_replica` | 3.979 +- 0.082 | 4.859 +- 0.080 (+22.1%) | 4.850 +- 0.098 (+21.9%) | 3.998 +- 0.072 (+0.5%) |
| `read_page100_replica` | 261.981 +- 5.296 | 264.061 +- 4.890 (+0.8%) | 267.865 +- 8.716 (+2.2%) | 264.133 +- 6.187 (+0.8%) |

Without the check the main tree reads at 7.27.4's speed: the check is all
of the price of a page of 1 row.

### Oct-2026: 7.27.1 against 7.27.0 (RelWithDebInfo, `CONFIG_DEBUG_TRACK_MEMORY` on)

7.27.1 changes only the external libraries (`linux-ext-libs` 1.24: OpenSSL
3.6.3 -> 3.6.5, PCRE2 10.47 -> 10.49; the standalone nginx is not linked into
any yuno). The 7.27.0 side was built from a git worktree of the tag with its
own `outputs_ext` (configure-libs 1.23, without nginx and openresty), the same
`.config` and compiler; only the kernel libraries, the modules, the benchmarks
below and three yunos were built there. The two binaries of each benchmark run
alternately, the order flipped every round, with a `sync` and a 3 s pause
before each run: 16 rounds (8 for `perf_auth_bff`). Mean +- standard
deviation; |t| < 3 is noise. No html report: a library bump ships none.

| Case | 7.27.0 | 7.27.1 | Change |
|------|--------|--------|--------|
| net.tcp_echo (round trips/s, control) | 34,873 +- 4,224 | 35,518 +- 1,771 | +1.8% (t = +0.6) |
| net.tls_echo (round trips/s) | 27,494 +- 1,224 | 27,259 +- 1,111 | -0.9% (t = -0.6) |
| net.tls_echo_persist (round trips/s) | 22,238 +- 1,099 | 22,436 +- 1,149 | +0.9% (t = +0.5) |
| net.auth_bff_login (logins/s) | 7,544 +- 43 | 7,477 +- 74 | -0.9% (t = -2.2) |

Stripped sizes (bytes): `auth_bff` 10,401,408 -> 10,409,600 (+8 KiB),
`mqtt_broker` 10,703,296 -> 10,707,392 (+4 KiB), `sgateway` 10,401,856 ->
10,414,144 (+12 KiB), `yuneta_agent` 10,747,040 -> 10,751,136 (+4 KiB).
No figure of the reports runs PCRE2 on its measured path (it is used by
`json_replace_vars()` at start-up, by the `rkey` filter of a timeranger2 list
and by a regex filter of `C_TRANGER`), and jansson did not change, so no regex
or json case was run.
The absolute figures sit below those of 7.27.0's own report on both sides: a
different moment of the same machine, not a change of either tree.

### Oct-2026: 7.27.0 against 7.26.9 (RelWithDebInfo, `CONFIG_DEBUG_TRACK_MEMORY` on)

Each release built from its own tree (a git worktree of the 7.26.9 tag, the
same `.config` and compiler; only the kernel libraries, the modules,
`performance/c/` and the three ctest binaries built there). `outputs_ext` was
shared: linux-ext-libs did not change (1.23 at both tags). The two binaries of
each benchmark run alternately, the order flipped every round, with a `sync`
and a 3 s pause before each run: 16 rounds (8, then 8 more: other work on the
machine during rounds 2 to 4 had widened the spreads); the three timeranger2
ctest binaries 24 rounds, a 1 s pause. Mean +- standard deviation; the treedb
figures are CPU time; |t| < 3 is noise. The report:
[`performance/reports/7.27.0.html`](../reports/7.27.0.html).

| Case | 7.26.9 | 7.27.0 | Change |
|------|--------|--------|--------|
| tr2.append_rate (appends/s) | 227,258 +- 3,329 | 229,502 +- 3,519 | +1.0% (t = +2.3) |
| tr2.append_rate_rt (appends/s) | 192,422 +- 4,394 | 193,449 +- 4,224 | +0.5% (t = +0.8) |
| tr2.build_appends (ms) | 1,576 +- 78 | 1,586 +- 74 | +0.7% (t = +0.4) |
| tr2.tm_build_appends (ms) | 1,644 +- 84 | 1,639 +- 71 | -0.3% (t = -0.2) |
| tr2.read_rate_iterator (records/s) | 218,766 +- 3,641 | 218,216 +- 3,899 | -0.3% (t = -0.5) |
| tr2.read_rate_pages (records/s) | 192,687 +- 9,098 | 197,017 +- 2,635 | +2.2% (t = +2.2) |
| tr2.open_master (ms) | 80.0 +- 2.8 | 82.2 +- 5.2 | +2.8% (t = +1.5) |
| tr2.tm_query (ms) | 12.7 +- 0.7 | 12.8 +- 0.9 | +0.9% (t = +0.4) |
| tr2.open_replica (ms) | 94.4 +- 6.0 | 94.5 +- 6.4 | +0.0% (t = +0.0) |
| tr2.create_topic (ms) | 143.5 +- 19.5 | 136.2 +- 21.2 | -5.1% (t = -1.0) |
| tr2.topic_version_change (ms) | 254.6 +- 27.4 | 261.3 +- 21.9 | +2.6% (t = +0.8) |
| treedb.update_memory (us) | 2.312 +- 0.092 | 2.461 +- 0.063 | +6.5% (t = +5.4) **price** |
| treedb.update_saved (us) | 7.548 +- 0.285 | 7.844 +- 0.638 | +3.9% (t = +1.7) |
| treedb.link_unlink (us) | 7.608 +- 0.303 | 7.760 +- 0.306 | +2.0% (t = +1.4) |
| treedb.create_link_half (us) | 65.8 +- 4.5 | 64.2 +- 2.0 | -2.4% (t = -1.3) |
| treedb.reopen (us) | 289.7 +- 12.2 | 290.8 +- 9.9 | +0.4% (t = +0.3) |
| treedb.delete_force (us) | 43.6 +- 2.3 | 42.7 +- 1.5 | -2.1% (t = -1.4) |
| treedb.delete_parent (us) | 2,354 +- 212 | 2,316 +- 111 | -1.6% (t = -0.6) |
| ctreedb.same_literal_release (s) | 0.372 +- 0.071 | 0.383 +- 0.071 | +2.9% (t = +0.4) |
| ctreedb.seed (s) | 10.8 +- 0.7 | 10.6 +- 0.7 | -1.6% (t = -0.7) |
| ctreedb.newer_literal (s) | 13.1 +- 0.9 | 13.0 +- 0.6 | -1.1% (t = -0.5) |
| rotatory.audit_record (ns) | 587.5 +- 22.7 | 578.0 +- 25.7 | -1.6% (t = -1.1) |
| rotatory.audit_record_flush (ns) | 1,256 +- 37 | 1,253 +- 45 | -0.2% (t = -0.2) |
| rotatory.log_record (ns) | 380.8 +- 10.6 | 386.3 +- 14.5 | +1.4% (t = +1.2) |
| net.yev_ping_pong (K msg/s) | 145.9 +- 4.1 | 144.9 +- 7.4 | -0.7% (t = -0.5) |
| net.yev_ping_pong_persist (K msg/s) | 84.7 +- 2.1 | 85.4 +- 3.0 | +0.7% (t = +0.7) |
| net.tcp_echo (round trips/s) | 38,367 +- 1,260 | 38,744 +- 1,063 | +1.0% (t = +0.9) |
| net.tcp_echo_persist (round trips/s) | 30,346 +- 1,436 | 29,986 +- 1,020 | -1.2% (t = -0.8) |
| net.tls_echo (round trips/s) | 28,832 +- 865 | 28,937 +- 1,500 | +0.4% (t = +0.2) |
| net.tls_echo_persist (round trips/s) | 23,741 +- 1,237 | 23,506 +- 1,038 | -1.0% (t = -0.6) |
| net.auth_bff_login (logins/s) | 8,338 +- 650 | 8,455 +- 286 | +1.4% (t = +0.7) |

One price, `treedb.update_memory`: the NUL checks of every treedb write
(`refuse_node_name_with_nul()`, the id and each pkey2 value;
`refuse_fkey_ref_with_nul()`, a walk of the kw before any fkey reference is
read). `perf` is not permitted on this machine, so it was attributed by
relinking `perf_tr_treedb`, 16 alternated rounds each (CPU us per operation,
mean +- standard deviation):

| Case | 7.26.9 | 7.26.9 timeranger2 + 7.27.0 gobj-c | 7.27.0 |
|------|--------|-----------------------------------|--------|
| `update_memory` | 2.296 +- 0.037 | 2.186 +- 0.046 (-4.8%) | 2.438 +- 0.081 (+6.2%, t = 6.3) |
| `update_saved` | 7.486 +- 0.153 | 7.301 +- 0.133 (-2.5%) | 7.655 +- 0.126 (+2.3%, t = 3.4) |

| Case | 7.26.9 | 7.27.0 `tr_treedb.c` as an object | the same, the two NUL checks taken out |
|------|--------|-----------------------------------|----------------------------------------|
| `update_memory` | 2.327 +- 0.097 | 2.415 +- 0.052 (+3.8%, t = 3.2) | 2.328 +- 0.047 (+0.0%) |
| `update_saved` | 7.529 +- 0.154 | 7.799 +- 0.189 (+3.6%, t = 4.4) | 7.502 +- 0.184 (-0.4%) |

The gobj-c of 7.27.0 is not the cost (the hybrid is faster than 7.26.9); the
two checks are all of it, about 0.1 us of CPU per write. The price of 7.26.9,
`net.yev_ping_pong_persist`, is not there against 7.26.9 (+0.7%).

### Oct-2026: 7.26.9 against 7.26.0 (RelWithDebInfo, `CONFIG_DEBUG_TRACK_MEMORY` on)

Written on 2026-10-22, after the release, against 7.26.0: the last release
with a study. It replaces the study made at the tag against 7.26.8, a release
nobody had measured; this one covers 7.26.1 to 7.26.9 (7.26.1 to 7.26.4
changed no code of the kernel libraries or the modules). Each release built
from a git worktree of its tag (same `.config` and compiler), the
`perf_timeranger2` of the main tree (with its `read` phase) compiled into
both. `outputs_ext` shared: linux-ext-libs 1.24 (OpenSSL 3.6.5) on both
sides, where both tags shipped with 1.23. The two binaries of each benchmark
run alternately, a `sync` and a 3 s pause before each run (1 s for the ctest
binaries and the read phase): 15 rounds (the first left out: it ran right
after a build on the machine); 31 for `perf_timeranger2` (47 for its appends,
opens and tm query), `perf_tr_treedb` and `perf_c_treedb`; 24 for
`perf_timeranger2 read` (a first run of 24 left out: a build of another tree
ran during it). Mean +- standard deviation; treedb figures are CPU time;
|t| < 3 is noise. The report:
[`performance/reports/7.26.9.html`](../reports/7.26.9.html).

| Case | 7.26.0 | 7.26.9 | Change |
|------|--------|--------|--------|
| tr2.append_rate (appends/s) | 222,265 +- 6,537 | 223,770 +- 6,593 | +0.7% (t = +0.6) |
| tr2.append_rate_rt (appends/s) | 188,600 +- 6,461 | 189,144 +- 6,582 | +0.3% (t = +0.2) |
| tr2.build_appends (ms) | 1,550 +- 46.31 | 1,586 +- 30.24 | +2.3% (t = +4.4) **price** |
| tr2.tm_build_appends (ms) | 1,604 +- 42.66 | 1,669 +- 43.69 | +4.1% (t = +7.3) **price** |
| tr2.read_rate_iterator (records/s) | 213,707 +- 7,689 | 215,615 +- 6,778 | +0.9% (t = +0.7) |
| tr2.read_rate_pages (records/s) | 190,887 +- 7,408 | 192,700 +- 7,104 | +0.9% (t = +0.7) |
| tr2.open_master (ms) | 79.84 +- 2.450 | 79.85 +- 2.409 | +0.0% (t = +0.0) |
| tr2.tm_query (ms) | 12.47 +- 0.364 | 12.51 +- 0.389 | +0.3% (t = +0.5) |
| tr2.open_replica (ms) | 93.11 +- 2.643 | 93.46 +- 2.661 | +0.4% (t = +0.7) |
| tr2.create_topic (ms) | 146.2 +- 15.06 | 147.8 +- 15.48 | +1.1% (t = +0.4) |
| tr2.topic_version_change (ms) | 264.3 +- 24.78 | 269.0 +- 23.26 | +1.8% (t = +0.8) |
| treedb.update_memory (us) | 2.368 +- 0.081 | 2.293 +- 0.155 | -3.2% (t = -2.4) |
| treedb.update_saved (us) | 7.636 +- 0.233 | 7.478 +- 0.276 | -2.1% (t = -2.4) |
| treedb.link_unlink (us) | 8.591 +- 0.238 | 7.544 +- 0.223 | -12.2% (t = -17.9) **gain** |
| treedb.create_link_half (us) | 65.23 +- 3.166 | 63.85 +- 3.255 | -2.1% (t = -1.7) |
| treedb.reopen (us) | 311.6 +- 10.01 | 282.8 +- 10.09 | -9.2% (t = -11.3) **gain** |
| treedb.delete_force (us) | 44.26 +- 2.454 | 42.64 +- 1.563 | -3.7% (t = -3.1) **gain** |
| treedb.delete_parent (us) | 2,536 +- 172.2 | 2,281 +- 131.7 | -10.1% (t = -6.5) **gain** |
| ctreedb.same_literal_release (s) | 0.355 +- 0.040 | 0.359 +- 0.049 | +1.2% (t = +0.4) |
| ctreedb.seed (s) | 10.29 +- 0.486 | 10.97 +- 0.457 | +6.7% (t = +5.7) **price** |
| ctreedb.newer_literal (s) | 13.18 +- 0.582 | 13.32 +- 0.616 | +1.1% (t = +0.9) |
| rotatory.audit_record (ns) | 585.2 +- 37.15 | 579.4 +- 25.55 | -1.0% (t = -0.5) |
| rotatory.audit_record_flush (ns) | 1,251 +- 47.35 | 1,263 +- 53.96 | +1.0% (t = +0.7) |
| rotatory.log_record (ns) | 380.3 +- 20.20 | 383.3 +- 19.85 | +0.8% (t = +0.4) |
| net.yev_ping_pong (K msg/s) | 147.1 +- 5.650 | 146.5 +- 6.240 | -0.4% (t = -0.3) |
| net.yev_ping_pong_persist (K msg/s) | 86.30 +- 2.124 | 86.17 +- 2.636 | -0.2% (t = -0.2) |
| net.tcp_echo (round trips/s) | 38,890 +- 943.1 | 39,034 +- 907.5 | +0.4% (t = +0.4) |
| net.tcp_echo_persist (round trips/s) | 30,338 +- 936.9 | 30,447 +- 968.3 | +0.4% (t = +0.3) |
| net.tls_echo (round trips/s) | 29,357 +- 680.1 | 29,440 +- 394.6 | +0.3% (t = +0.4) |
| net.tls_echo_persist (round trips/s) | 23,650 +- 600.2 | 23,982 +- 524.2 | +1.4% (t = +1.6) |
| net.auth_bff_login (logins/s) | 8,546 +- 300.1 | 8,527 +- 228.6 | -0.2% (t = -0.2) |

**The read path** (`perf_timeranger2 read`, us per operation, 24 rounds):

| Case | 7.26.0 | 7.26.9 | Change |
|------|--------|--------|--------|
| `read_page1_replica` | 5.068 +- 0.093 | 5.114 +- 0.099 | +0.9% (t = +1.6) |
| `read_page1_filtered_replica` | 3.970 +- 0.072 | 3.940 +- 0.076 | -0.7% (t = -1.4) |
| `read_open_replica` | 15.94 +- 0.593 | 15.95 +- 0.580 | +0.1% (t = +0.1) |
| `read_open_filtered_replica` | 11.32 +- 0.321 | 11.56 +- 0.454 | +2.1% (t = +2.1) |
| `read_page100_replica` | 264.4 +- 5.133 | 263.2 +- 6.477 | -0.5% (t = -0.7) |
| `read_page1_master` | 5.016 +- 0.100 | 5.123 +- 0.217 | +2.1% (t = +2.2) |
| `read_page1_filtered_master` | 3.956 +- 0.085 | 3.951 +- 0.093 | -0.1% (t = -0.2) |
| `read_open_master` | 15.55 +- 0.290 | 15.78 +- 0.703 | +1.5% (t = +1.5) |
| `read_open_filtered_master` | 11.11 +- 0.317 | 11.67 +- 1.299 | +5.0% (t = +2.0) |
| `read_page100_master` | 264.6 +- 5.962 | 260.6 +- 5.677 | -1.5% (t = -2.4) |

**Where each move appeared.** The figures that moved, measured again on the
tags and commits between, every build in one rotation, 16 rounds:

| Case | 7.26.0 | 7.26.6 | 7.26.7 | 7.26.8 | 7.26.9 |
|------|--------|--------|--------|--------|--------|
| `ctreedb.seed` (s) | 10.24 | 10.33 | 10.98 | -- | 10.96 |
| `tr2.tm_build_appends` (ms) | 1,596 | 1,591 | 1,594 | 1,597 | 1,656 |
| `tr2.build_appends` (ms) | 1,543 | 1,536 | 1,539 | 1,541 | 1,582 |

| Case | 7.26.8 | `df6c5f06a` | `ac9165b82` | `6193b002c` | `9d8f7c584` | `f4be7d750` | 7.26.9 |
|------|--------|-------------|-------------|-------------|-------------|-------------|--------|
| `tr2.tm_build_appends` (ms) | 1,583 | 1,598 | 1,600 | 1,627 | 1,637 | -- | 1,655 |
| `tr2.build_appends` (ms) | 1,525 | 1,535 | 1,534 | 1,542 | 1,555 | -- | 1,578 |
| `treedb.link_unlink` (us) | 8.623 | -- | -- | -- | 8.283 | 7.528 | 7.381 |
| `treedb.delete_parent` (us) | 2,506 | -- | -- | -- | 2,411 | 2,262 | 2,264 |
| `treedb.reopen` (us) | 309.5 | -- | -- | -- | 286.7 | 281.8 | 278.3 |

(The treedb rows of 7.26.0 against 7.26.8 in the same rotation: link or
unlink 8.51 against 8.68 us, reopen 310.7 against 309.7 us: 7.26.1 to 7.26.8
did not move them.)

The gains are the treedb names looked up by key (`KWID_GET_BY_KEYS()`):
**appeared in 7.26.9**, a link or unlink and the delete of a parent mostly
with `f4be7d750` (*names looked up by key*), a reopen mostly before it, by
`9d8f7c584` (the commits that read a name from data as a key).
`ctreedb.seed` (+6.7%) **appeared in 7.26.7, commit `50c6cd78f`** (its only C
change): every treedb gets a system topic `__icons__` at its first open, 40
topics more for 40 treedbs, each with its fsyncs; the everyday open did not
move. The appends of `perf_timeranger2` (+4.1% for 600 000 to one key, +2.3%
for 400 000 into 20 000 files) **appeared in 7.26.9, in steps**: about +1%
with the kw readers that answer what the kw holds (`df6c5f06a`,
`ac9165b82`), +1.7% more with `6193b002c` (a name that comes from data read
as a key), +0.6% with `9d8f7c584`, the rest after it -- no commit carries it
alone; about 0.11 us per append, the price of the hardened kw calls. The main
append benchmark moved +0.7% (t = 0.6). The study made at the tag reported the
event loop echo with an append 3.9% slower against 7.26.8 and could not trace
it: against 7.26.0 it is -0.2% (t = -0.2).

### Oct-2026: 7.26.0 against 7.25.22 (RelWithDebInfo, `CONFIG_DEBUG_TRACK_MEMORY` on)

Each release built from its own tree (a git worktree of the 7.25.22 tag, the
same `.config` and compiler; only the kernel libraries, the modules,
`performance/c/` and the three ctest binaries built there). `outputs_ext` was
shared: linux-ext-libs did not change. The two binaries of each benchmark run
alternately, the order flipped every round, with a `sync` and a 3 s pause
before each run: 8 rounds, 24 for the three timeranger2 ctest binaries. Mean
+- standard deviation; the treedb figures are CPU time. The report:
[`performance/reports/7.26.0.html`](../reports/7.26.0.html).

| Case | 7.25.22 | 7.26.0 | Change |
|------|---------|--------|--------|
| `test_topic_pkey_integer`, appends/s | 219,597 +- 8,043 | 223,864 +- 12,198 | +1.9% (noise, t = 1.4) |
| the same, with an rt list | 186,150 +- 7,455 | 189,539 +- 7,784 | +1.8% (noise) |
| `perf_timeranger2` `build_appends`, `tm_build_appends` (ms) | 1645.9, 1680.3 | 1615.2, 1693.3 | noise |
| `perf_timeranger2` `open_master` (ms) | 80.5 +- 2.1 | 81.9 +- 1.9 | noise |
| `perf_timeranger2` `open_replica` (ms) | 108.4 +- 3.6 | 94.8 +- 2.7 | -12.6% (GAIN: no tm marker looked for) |
| `perf_timeranger2` tm query, a topic 7.25.22 did not mark (ms) | 401.6 +- 24.5 | 12.8 +- 0.9 | -96.8%, 31x (GAIN: block reads, condition parsed once) |
| `perf_timeranger2` tm query, a topic 7.25.22 marked (ms) | 7.55 +- 0.31 | 12.8 +- 0.9 | +69.9% (PRICE: no tm markers, every row read) |
| `perf_timeranger2` `create_topic` (ms, 10 topics) | 131.4 +- 18.0 | 134.0 +- 10.0 | +2.0% (noise; 258.5 +- 23.9 with `delete_seq.json` written durably, fixed before the tag) |
| `perf_timeranger2` `topic_version_change` (ms) | 237.1 | 253.7 | +7.0% (noise, t = 1.5) |
| reads, iterator (records/s) | 184,338 +- 5,063 | 215,157 +- 7,268 | +16.7% (GAIN) |
| reads, pages (records/s) | 166,017 +- 5,199 | 192,697 +- 4,480 | +16.1% (GAIN) |
| `perf_tr_treedb` `update_memory`, `update_saved`, `link_unlink` (us) | 2.424, 7.812, 8.686 | 2.489, 7.799, 8.649 | noise |
| `perf_tr_treedb` `reopen` (us) | 390.3 +- 11.5 | 311.2 +- 10.2 | -20.3% (GAIN: the same scans) |
| `perf_tr_treedb` `create_link_half`, `delete_force`, `delete_parent` | 67.0, 42.4, 2547 | 65.6, 43.7, 2563 | noise |
| `perf_c_treedb` `same_literal`, `newer_literal` (s) | 0.341, 13.23 | 0.335, 13.22 | noise |
| `perf_c_treedb` `seed` (s, 400 topics) | 10.42 +- 0.50 | 10.33 +- 0.48 | -0.9% (noise; 16.19 +- 0.58 durably, fixed before the tag) |
| `perf_rotatory` audit / flushed / log (ns) | 555 / 1253 / 373 | 570 / 1257 / 374 | noise |
| `perf_yev_ping_pong` / `2` (K msg/s) | 151.4 / 84.7 | 149.0 / 86.1 | noise (t = -2.0 / 1.9) |
| `perf_tcp_test4` / `5` (round trips/s) | 39,014 / 30,441 | 39,511 / 30,378 | noise |
| `perf_tcps_test4` / `5` (round trips/s) | 29,278 / 23,746 | 29,762 / 23,907 | noise |
| `perf_auth_bff` (logins/s) | 8,572 | 8,622 | noise |

The gains come from the scans of timeranger2: the md2 rows read 1024 at a time
with one `pread()`, and the match condition parsed once per scan. The prices:
the tm markers are gone (a tm query of a topic 7.25.22 had marked reads every
row again). A master that opened a topic wrote its `delete_seq.json` durably
(a file fsync, a rename, a directory fsync), once per topic, which doubled
`create_topic` and made `seed` 55% longer: it holds 0 and is made again if
lost, so it is written not durably since, and the two rows above are the 7.26.0
side measured again after that fix (8 rounds, not alternated). Not measured: a
key delete on a topic with an rt_disk feed (no benchmark covers it), which by
construction writes the record of its sequence first.

### Oct-2026: 7.25.22 against 7.25.21 (RelWithDebInfo, `CONFIG_DEBUG_TRACK_MEMORY` on)

Each release built from its own tree (a git worktree of the 7.25.21 tag, the
same `.config` and compiler; only the kernel libraries, `modules/c/test`,
`performance/c/` and the three ctest binaries built there). `outputs_ext` was
shared: linux-ext-libs did not change. The two binaries of each benchmark run
alternately, the order flipped every round, with a `sync` and a 3 s pause
before each run: 8 rounds, 24 for the three timeranger2 ctest binaries. Mean
+- standard deviation. The report:
[`performance/reports/7.25.22.html`](../reports/7.25.22.html).

| Case | 7.25.21 | 7.25.22 | Change |
|------|---------|---------|--------|
| `test_topic_pkey_integer`, appends/s | 228,532 +- 3,220 | 224,185 +- 3,339 | -1.9% (placement, see below) |
| the same, with an rt list | 194,479 +- 2,672 | 191,447 +- 3,677 | -1.6% (placement, see below) |
| `perf_timeranger2` `build_appends` (ms) | 1708.5 +- 32.5 | 1564.1 +- 25.4 | -8.5% (placement, see below) |
| `perf_timeranger2` `tm_build_appends` (ms) | 1616.3 +- 40.2 | 1654.5 +- 29.3 | +2.4% (noise, t = 2.2) |
| `perf_timeranger2` `open_master`, `open_replica` (ms) | 80.3, 107.1 | 79.6, 105.1 | noise |
| `perf_timeranger2` `tm_query_migrated`, `unmigrated` (ms) | 7.53, 391.0 | 7.47, 392.8 | noise |
| `perf_timeranger2` `create_topic`, `topic_version_change` (ms) | 143.3, 261.0 | 150.7, 262.8 | noise |
| reads, iterator / pages (records/s) | 188,059 / 169,137 | 189,390 / 169,121 | noise |
| `perf_tr_treedb` `update_memory` (us, CPU) | 2.297 +- 0.031 | 2.413 +- 0.025 | +5.0% (placement, see below) |
| `perf_tr_treedb` `update_saved`, `link_unlink` | 7.65, 8.54 | 7.75, 8.63 | noise |
| `perf_tr_treedb` `delete_force` (us) | 69.8 +- 1.0 | 42.5 +- 0.7 | -39.1% (GAIN, see below) |
| `perf_tr_treedb` `create_link_half`, `reopen`, `delete_parent` | 63.9, 385.6, 2548 | 63.3, 391.5, 2507 | noise |
| `perf_c_treedb` `same_literal`, `seed`, `newer_literal` (s) | 0.331, 10.04, 12.94 | 0.333, 10.12, 12.74 | noise |
| `perf_rotatory` audit / flushed / log (ns) | 565 / 1239 / 371 | 568 / 1232 / 369 | noise |
| `perf_yev_ping_pong` / `2` (K msg/s) | 152.1 / 84.0 | 150.5 / 84.8 | noise (t = -2.2 / 0.9) |
| `perf_tcp_test4` / `5` (round trips/s) | 38,908 / 30,948 | 38,292 / 30,570 | noise |
| `perf_tcps_test4` / `5` (round trips/s) | 29,441 / 24,156 | 29,663 / 24,528 | noise |
| `perf_auth_bff` (logins/s) | 8,769 | 8,581 | noise |

One gain, with its cause: a forced treedb delete takes 39% less time, because
a delete no longer walks every key with an open file to close the descriptors
of the one deleted (`tranger2_delete_key()` closes that key's own). Four
figures moved outside their spread on code that did not change in this
release, and none is claimed: the appends of `test_topic_pkey_integer` (-1.9%,
t = -4.6; -1.6% with an rt list, t = -3.3) and `build_appends` (-8.5% of time,
t = -9.9) run the same `tranger2_append_record()` and callees in both
releases, one slower and one faster; `update_memory` (+5.0%, t = 8.2) runs
`tr_treedb.c`, `kwid.c` and jansson, unchanged, and moved -5.2% the other way
in the A/B of 7.25.21. Whole-release builds move where the code lands:
placement. The C_TCP liveness markers (one stack marker per publish) did not
move the TCP or TLS echoes beyond their noise.

### Oct-2026: 7.25.21 against 7.25.20 (RelWithDebInfo, `CONFIG_DEBUG_TRACK_MEMORY` on)

Each release built from its own tree (a git worktree of the 7.25.20 tag, the
same `.config` and compiler; only the kernel libraries, `modules/c/test`,
`performance/c/` and the three ctest binaries built there). `outputs_ext` was
shared: linux-ext-libs moved 1.22 -> 1.23 in this release, only in how
openresty is downloaded, no library changed version. The two binaries of each
benchmark run alternately, the order flipped every round, with a `sync` and a
3 s pause before each run: 8 rounds, 24 for the three timeranger2 ctest
binaries. Mean +- standard deviation. The report:
[`performance/reports/7.25.21.html`](../reports/7.25.21.html).

| Case | 7.25.20 | 7.25.21 | Change |
|------|---------|---------|--------|
| `test_topic_pkey_integer`, appends/s | 223,031 +- 8,202 | 225,883 +- 6,043 | +1.3% (noise) |
| the same, with an rt list | 188,797 +- 5,244 | 191,672 +- 5,799 | +1.5% (noise) |
| `perf_timeranger2` `build_appends` (ms) | 1783.8 +- 29.3 | 1757.9 +- 50.9 | -1.5% (noise) |
| `perf_timeranger2` `tm_build_appends` (ms) | 1719.9 +- 45.5 | 1656.3 +- 33.4 | -3.7% (placement, see below) |
| `perf_timeranger2` `open_master`, `open_replica` (ms) | 82.4, 108.3 | 81.9, 107.1 | noise |
| `perf_timeranger2` `tm_query_migrated`, `unmigrated` (ms) | 7.47, 397.8 | 7.66, 399.4 | noise |
| `perf_timeranger2` `create_topic`, `topic_version_change` (ms) | 135.1, 242.4 | 133.7, 241.0 | noise |
| reads, iterator / pages (records/s) | 185,565 / 167,048 | 186,284 / 168,516 | noise |
| `perf_tr_treedb` `update_memory` (us, CPU) | 2.449 +- 0.142 | 2.322 +- 0.092 | -5.2% (noise, t = -2.1) |
| `perf_tr_treedb` `update_saved`, `link_unlink` | 7.82, 8.72 | 7.77, 8.71 | noise |
| `perf_tr_treedb` `create_link_half`, `reopen`, `delete_force`, `delete_parent` | 66.2, 387.4, 71.4, 2648 | 67.1, 391.1, 71.3, 2665 | noise |
| `perf_c_treedb` `same_literal`, `seed`, `newer_literal` (s) | 0.341, 10.03, 13.03 | 0.340, 10.16, 12.46 | noise |
| `perf_rotatory` audit / flushed / log (ns) | 593 / 1265 / 383 | 593 / 1267 / 381 | noise |
| `perf_yev_ping_pong` / `2` (K msg/s) | 147.9 / 84.9 | 147.1 / 84.1 | noise |
| `perf_tcp_test4` (round trips/s) | 37,909 +- 746 | 39,715 +- 349 | +4.8% (placement, see below) |
| `perf_tcp_test5` (round trips/s) | 29,505 | 30,173 | +2.3% (noise) |
| `perf_tcps_test4` / `5` (round trips/s) | 29,125 / 23,160 | 28,908 / 23,336 | noise |
| `perf_auth_bff` (logins/s) | 8,350 +- 157 | 8,606 +- 166 | +3.1% (placement, see below) |

Nothing is slower beyond its noise. Three figures moved outside their spread,
all faster, and none is claimed as a gain: `perf_tcp_test4` +4.8% (t = 6.2),
`perf_auth_bff` +3.1% (t = 3.2) and `tm_build_appends` -3.7% (t = -3.2). No
change of 7.25.21 takes work off those paths: a TCP send gained one key lookup
(`__secret__` in `EV_TX_DATA`), the send-to-all change of `C_IOGATE` is not used
by the echo, `c_auth_bff.c` changed by one line, and `tranger2_append_record()`
is the same in both releases (`tm_build_appends` moved +3.6% the other way in
the 7.25.20 report). The TLS echo, which runs the same `C_TCP` code, moved
-0.7%. Whole-release builds move where the code lands: placement. The masking
of secrets runs only when a trace prints; a secret gbuffer costs one flag test
on its free and its growth.

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
