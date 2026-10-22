# Round 1: the core of gobj-c

Scope: the allocator (`gbmem.c`), the publication of events, subscriptions,
`gobj_change_state()` and `gobj_match_gobj()` (`gobj.c`), with the two
benchmarks that cross them most: `perf_tcp_test4` (a ~140-byte JSON echoed
through `C_IOGATE` -> `C_CHANNEL` -> `C_PROT_TCP4H` -> `C_TCP`, 180 000 round
trips) and `perf_tr_treedb` (treedb writes, 100 000 operations).

## What the profile of the base showed

`callgrind`, instructions of the whole run (7.27.7, `1d827e4`):

| | `perf_tcp_test4` | `perf_tr_treedb` |
|---|---|---|
| Total | 41.14 G | 30.35 G |
| The allocator (`calloc`, `_int_malloc`, `_int_free`, `free`) | ~32% | ~22% |
| Key lookups of jansson (`hashlittle`, `hashtable_get`, `json_object_get`, `strlen`, `memcmp`) | ~22% | ~32% |
| `C_TIMER` armed and cleared (`set_timeout`/`clear_timeout`): its `gobj_subscribe_event()` / `gobj_unsubscribe_event()` on the yuno | 28% | - |
| `gobj_change_state()` | 15% | - |
| `gobj_match_gobj()` (`C_IOGATE` choosing a channel) | 7% | - |

Four things stand out:

1. **Every allocation is a `calloc()`.** `_mem_malloc()` used `calloc()`
   for every block, jansson's included (an object's pair, an integer, a
   string). Up to glibc 2.40, `calloc()` never takes a block from the
   per-thread cache (tcache): every small `calloc()` goes down to
   `_int_malloc`, and the cache that `free()` fills is never emptied by it,
   so `free()` overflows into `_int_free`. glibc 2.41 gave `calloc()` its
   tcache path (`__libc_calloc` calls `tcache_try_malloc` in 2.41, not in
   2.39). A microbenchmark on this machine (glibc 2.39): `calloc()` costs
   twice `malloc()` + `memset()` between 128 and 1024 bytes, the same below
   96.
2. **A publication reads every subscription through paths.** For each
   subscription `gobj_publish_event()` reads 8 fields with `kw_get_*()`:
   a path search, a copy of the segment, then the lookup. The keys are
   literals without delimiter. `_find_subscriptions()` (every subscribe and
   unsubscribe) read the 4 fields of the kw to match once per subscription
   of the list, and the yuno's list holds one subscription per running
   timer.
3. **`EV_STATE_CHANGED` is built for nobody.** A protocol changes state twice
   a frame (`C_PROT_TCP4H`: wait the header, wait the payload). Each change
   built a kw of two strings and published it; the only subscriber, the
   parent (CHILD subscription model), does not declare the system event, so
   the publication skipped it after copying and reading the subscription
   list.
4. **`gobj_match_gobj()` deep-copies its filter on every call**, to delete
   from the copy the keys it answers itself. `C_IOGATE` calls it for every
   message it sends, once per channel tried.

The biggest single cost, the timer armed and cleared on every frame, is not
in `gobj-c`: it is the protocols' use of `C_TIMER`, left for round 3 (the
network path). Round 1 makes the subscription machinery it runs on cheaper.

## Changes applied (one commit each)

| Commit | Change | What stays the same |
|--------|--------|---------------------|
| `perf(gbmem): small blocks from malloc() + memset(), not calloc()` | A block of up to 1024 bytes is `malloc()` + `memset()`; larger ones keep `calloc()` (it skips the memset of fresh memory). Only under `__GLIBC__`. | Every block is still zeroed. |
| `perf(gobj): read subscription fields without a path` | `subs_get_int()` / `subs_get_dict()` read a field with `json_object_get()`; `_find_subscriptions()` reads the kw to match once, not once per subscription. | Any field of another shape goes to `kw_get_int()`, which converts and logs as before. A kw that is not a dict or a list (whose reads log) is still read per subscription, so the logs are the same. |
| `perf(gobj): no EV_STATE_CHANGED kw for a change nobody can see` | `state_changed_has_audience()` skips the kw and the publication only when the publication would show nothing. | It answers "nobody" only when: the gclass has no `mt_state_changed`, `mt_publish_event` or publication filter; no trace of the publication is on; the event is the global system one with `EVF_NO_WARN_SUBS` (no warning); and every subscription is skipped exactly as `gobj_publish_event()` skips it. A `__filter__`, or a field the publication would log about, takes the change down the publication as before. |
| `perf(gobj): gobj_match_gobj() without a deep copy of the filter` | The attribute loop skips the keys that name the gobj instead of deleting them from a copy. | A key is skipped exactly when it was deleted (a string was read for it, or it is `__disabled__`); the filter is not modified. |

## Results

### Instructions (`callgrind`, deterministic)

| Benchmark | Base | Round 1 | Change |
|-----------|------|---------|--------|
| `perf_tcp_test4` | 41.14 G | 27.58 G | **-33.0%** |
| `perf_tr_treedb` | 30.35 G | 27.89 G | **-8.1%** |

Where the 13.6 G of the TCP echo went:

| Function (inclusive) | Base | Round 1 |
|---|---|---|
| `calloc` + `free` (base) / `malloc` + `free` (round 1) | 12.70 G | 4.88 G |
| `gobj_change_state()` | 6.13 G | 1.10 G |
| `gobj_match_gobj()` | 2.91 G | 0.99 G |
| `gobj_subscribe_event()` + `gobj_unsubscribe_event()` | 10.50 G | 8.41 G |

### Time (A/B, alternated)

8 rounds, the order turned every round, a `sync` and a 2 s pause before each
run; mean ± standard deviation, t = the change over its standard error.

| Benchmark | Base | Round 1 | Change | t |
|-----------|------|---------|--------|---|
| TCP echo through the gobj stack (`perf_tcp_test4`, round trips/s) | 39 651 ± 1 577 | 52 330 ± 2 906 | **+32.0%** | 10.8 |
| TCP echo + one append per message (`perf_tcp_test5`) | 28 496 ± 976 | 35 758 ± 1 736 | **+25.5%** | 10.3 |
| TLS echo, OpenSSL (`perf_tcps_test4`) | 28 143 ± 1 328 | 35 416 ± 2 380 | **+25.8%** | 7.5 |
| BFF logins, 5 parallel clients (`perf_auth_bff`, K/s) | 7.02 ± 0.31 | 7.87 ± 0.45 | **+12.1%** | 4.4 |
| treedb: update in memory (`perf_tr_treedb`, CPU µs) | 2.93 ± 0.35 | 2.63 ± 0.20 | -10.5% | 2.2 |
| treedb: update and save | 9.34 ± 1.70 | 8.76 ± 0.91 | -6.2% | 0.9 |
| treedb: link or unlink | 8.63 ± 0.86 | 7.92 ± 0.69 | -8.3% | 1.8 |
| timeranger2: appends/s (`test_topic_pkey_integer`) | 176 620 ± 12 682 | 184 593 ± 13 265 | +4.5% | 1.2 |
| timeranger2: records read/s, iterator (`..._iterator2`) | 162 899 ± 10 439 | 174 113 ± 7 978 | +6.9% | 2.4 |

The network figures move far outside their noise. The treedb and
timeranger2 ones move the way the instruction count says (-8%), but inside
the noise of this VM (|t| < 3). The rest of `perf_timeranger2`,
`perf_c_treedb` and `perf_rotatory` did not move beyond their noise.
`perf_rotatory` showed +13% on one case at t = 2.2; it runs the same
instructions in both builds (1.3175 G and 1.3174 G: the log path does not
cross the code changed) and 16 more rounds of it alone gave -3% to +1%,
all |t| ≤ 1.

The gain of the allocator is for binaries built against glibc ≤ 2.40
(this machine: 2.39; Rocky 9, where the `.rpm` packages and the binaries
built on the central node come from: 2.34). A binary built against glibc
≥ 2.41 (Debian 13, which builds the `.deb`; the development machine, 2.43)
already had the tcache in `calloc()`, so it keeps the other three changes
and not that one.

### Nothing changed

The whole ctest suite, 316 tests, before and after. On this VM 16 fail
with the change; with `fs.inotify.max_user_instances` raised from 128 (the
suite runs as root, so every test shares one inotify budget) 9 of them
remain, and **the base fails the same 9**, with the same messages (260
lines of failure, identical but for a date written inside one of them).
They are the environment: two `yev_events/test_yevent_listen*` need
`telnet`; `test_tr_treedb_failed_save`, `_delete_instance`, `_snap`,
`secret_attrs` and three `test_c_treedb_literal_wins` provoke a failed
write or an untrusted file by permissions, which root does not see.

## Found, not done

- **The timer of every frame** (28% of the instructions of the TCP echo in
  the base, 34% of what is left after round 1): `C_PROT_TCP4H`, `C_WEBSOCKET` and `C_PROT_MQTT2` arm a `C_TIMER` when
  a frame header arrives and clear it when its payload does, usually in the
  same read; each start subscribes the timer to the yuno's
  `EV_TIMEOUT_PERIODIC` and each stop unsubscribes it. Round 3 looks at
  avoiding the arm/clear when the payload is already there. A deeper fix,
  that a `C_TIMER` keeps its subscription to the yuno for its life, would
  change the list of subscriptions anybody can see: a proposal, not applied.
- **jansson's hash** (`hashlittle`) is 10-12% of both benchmarks, with
  `strlen` and `memcmp` around it: every `json_object_get()` hashes its key.
  A faster seeded hash in jansson (a vendored patch) keeps the order of
  keys (jansson iterates an object by its `ordered_list`, in insertion order,
  apart from the buckets) and the results. Not applied: it is a patch of a third-party library, the user's
  call.
- **Subscriptions as json.** A subscription is a json dict of 10 keys, built
  with `gobj_sdata_create()` (10 `json_object_set_new()`, then 4 of them
  overwritten). C structs would be far cheaper, but
  `gobj_find_subscriptions()` returns them as json: an API change.
