# Round 2: timeranger2 and tr_treedb

Scope: the append, the read of a record, the cache of the keys and the deletes
of `timeranger2.c`, and the writes of `tr_treedb.c`, with the benchmarks that
cross them: `test_topic_pkey_integer` (appends), `..._iterator2` (reads with an
iterator), `perf_timeranger2` (open, appends, tm query, pages) and
`perf_tr_treedb` (treedb writes). Measured on top of round 1: what is compared
here is round 1 against round 1 + round 2.

## What the profile showed

`callgrind` on round 1 (instructions):

| Where | Share |
|---|---|
| **jansson**, writing a record (`json_dumps`: `dump_string`, `utf8_iterate`, `utf8_check_*`) | ~50% of an append |
| **jansson**, reading a record (`json_loadb`: `stream_get`, `lex_scan`, `strbuffer_*`) | **82%** of a read |
| The cache of the keys on every append (`update_cache_cell`, `update_totals_of_key_cache2`, `set_cache_int`) | 19% of `perf_timeranger2`, 8% of an append |
| `tranger2_dict_topic_desc_cols()`, asked 3-4 times per treedb write | ~7% of `perf_tr_treedb` |
| `md2json()`, the `__md_tranger__` of every record read and realtime | 4-8% |
| `snprintf()` naming files (`"%s.%s"`) and integer keys (`"%019"PRIu64`) | ~3% of an append |
| `/proc/self/stat` read at every delete of a key | ~5 us of a 52 us forced delete (already measured in the study of 7.27.7) |

The cost of a record is jansson's: the code of timeranger2 around it is the
smaller part. What jansson costs is a separate proposal (below), because it is
a patch of a third-party library.

## Changes applied (one commit each)

| Commit | Change | What stays the same |
|--------|--------|---------------------|
| `perf(timeranger2): the cols of a topic without formatting a path` | `tranger2_dict_topic_desc_cols()` returns the `cols` dict increfed directly, instead of `kwid_new_dict()` (`vsnprintf` + `split2`, several allocations). | `kwid_new_dict()` answers the same dict increfed for a dict; a list (an older schema) or anything else still goes through it. |
| `perf(timeranger2): look up the cache integers once per append` | `get_cache_range()` looks the five integers of a cell (or of the totals) up once; `put_cache_int()` writes through the pointer. 20 lookups per append become 10. | The same values, written in place when an integer is there, else set new as `set_cache_int()` did, in the same order of keys. |
| `perf(timeranger2): md2json() without the UTF-8 check of its keys` | `json_object_set_new_nocheck()` for the 8 literal keys. | The keys are ASCII literals: the same object. |
| `perf(timeranger2): file names and integer keys without snprintf()` | `build_file_name()` and `build_int_key()`. | The same bytes, truncation included (a NULL file id as `"(null)"`, at least 19 digits zero padded): checked against `snprintf()` on 2 million cases, 0 differences. |
| `perf(timeranger2): read a record's content with pread()` | One `pread()` instead of `lseek()` + `read()`. | Nothing reads that descriptor by its position (the appends write through another one; every other read of a content uses `pread()`), and the offset is checked beforehand against the size of the file. |
| `perf(timeranger2): the start time of the process read once` | `own_process_start_time()`: `/proc/self/stat` read once per pid. | The start time of a process does not change while it lives; a child after a `fork()` reads its own, and an unknown start (0) is asked again, as before. |

## Results

### Instructions (`callgrind`, deterministic), round 1 -> round 2

| Benchmark | Round 1 | Round 2 | Change |
|-----------|---------|---------|--------|
| `perf_tr_treedb` | 27.89 G | 25.46 G | **-8.7%** |
| `perf_timeranger2` | 43.07 G | 39.51 G | **-8.3%** |
| `test_topic_pkey_integer` (appends) | 20.10 G | 18.85 G | **-6.2%** |
| `test_topic_pkey_integer_iterator2` (reads) | 11.51 G | 11.21 G | -2.6% (plus the syscall `pread()` saves, which callgrind does not count) |

Against the base (7.27.7), rounds 1 and 2 together: `perf_tr_treedb` 30.35 G
-> 25.46 G, **-16.1%**.

### Time (A/B, alternated), round 1 -> round 2

10 rounds, the order turned every round, a `sync` and a 2 s pause before each
run; mean ± standard deviation, t = the change over its standard error.

| Benchmark | Round 1 | Round 2 | Change | t |
|-----------|---------|---------|--------|---|
| Appends/s (`test_topic_pkey_integer`, without an rt list) | 187 264 ± 10 624 | 216 558 ± 11 540 | **+15.6%** | 5.9 |
| Appends/s, with a live reader (rt list) | 164 721 ± 5 292 | 191 629 ± 6 398 | **+16.3%** | 10.2 |
| Records read/s, iterator with callback (`..._iterator2`) | 175 352 ± 8 986 | 199 011 ± 21 005 | **+13.5%** | 3.3 |
| treedb: update and save (`perf_tr_treedb`, CPU µs) | 8.40 ± 0.69 | 7.34 ± 0.45 | **-12.6%** | 4.1 |
| treedb: forced delete | 105.6 ± 12.0 | 86.6 ± 6.3 | **-18.0%** | 4.4 |
| treedb: reopen (per node) | 328.9 ± 13.5 | 295.8 ± 21.1 | **-10.1%** | 4.2 |
| treedb: link or unlink | 8.24 ± 0.38 | 7.65 ± 0.52 | -7.1% | 2.9 |
| treedb: update in memory | 2.57 ± 0.22 | 2.30 ± 0.38 | -10.4% | 2.0 |
| A page of 100 rows, replica (`perf_timeranger2`, µs) | 271.9 ± 26.3 | 241.5 ± 16.2 | **-11.2%** | 3.1 |
| A page of 100 rows, master | 269.7 ± 25.4 | 244.7 ± 20.9 | -9.3% | 2.4 |
| Pages read/s, forward / backward (`..._iterator5`) | 159 761 / 165 962 | 179 152 / 181 192 | +12.1% / +9.2% | 2.3 / 2.5 |

The rest of `perf_timeranger2` (open, create, tm query, pages of 1 row),
`perf_c_treedb` and the TCP echo with an append per message moved inside
their noise. The appends gained more time (+16%) than instructions (-6%);
why was not measured here (`callgrind` counts instructions, not cache misses
nor time in the kernel), so the time is the figure to take.

### Nothing changed

The whole ctest suite, 316 tests: the same 9 failures as the base, all of
the environment (see round 1). The failures of the treedb tests among them
were compared message by message with the base: 208 lines, identical.

## Found, not done

- **jansson is most of the cost of a record**, in both directions. Three
  fast paths for ASCII would leave every result, every byte and every error
  exactly as they are: see the proposal below.
- **The realtime lists of the treedb get a `__md_tranger__` they do not
  read.** `tranger2_append_record()` builds `md2json()` for every list that
  wants the record, and the treedb's own lists (`load_id_callback`,
  `load_pkey2_callback`) do nothing with a realtime record. Marking them
  `only_md` after the load would skip it (~5 000 instructions per save), but
  `only_md` would show in the list's `match_cond` and the callback would get
  NULL: a visible change, so a proposal.
- **Hooks rebuilt at load are O(children^2).** `link_child_to_parent()` ->
  `child_in_hook_array()` scans the whole hook array for every link loaded
  (two hash lookups and a `strcmp` per entry). A parent with C children costs
  C^2/2 comparisons at every open: 200 children, ~1% of the benchmark's
  reopen; 3 000 children (one parent per zone with all the devices of a
  node), about 4.5 million comparisons, of the order of half a second of
  start-up. Removing it keeps the results (a temporary index of each hook
  array during the load) but adds a structure to a consolidated module: left
  for a decision.
- **The flags of the columns** are read with `kw_has_word()` (a `strstr()`
  in the list of flags) for every column of every write (~3% of a treedb
  write). Caching them per topic would need invalidating the cache when the
  columns change: not done.
- **The reopen of the treedb loads the whole history of every key.** The
  load of the ids walks every record backwards and keeps the first of each
  key: 200 000 records parsed for 2 000 nodes in `perf_tr_treedb` (its
  `reopen` measures the length of the history, not the number of nodes).
  Stopping at the newest record of each key would make the open O(keys), but
  a damaged older record would no longer be detected at open: a functional
  change, a proposal.

## Proposal: jansson, fast paths for ASCII

The patch is
[`proposals/jansson-0002-ascii-fast-paths.patch`](proposals/jansson-0002-ascii-fast-paths.patch),
with its description and how it was checked in its header. It is **not
applied**: it is a patch of a third-party library, it bumps the `VERSION` of
`linux-ext-libs` (every node that builds from source reruns `extrae.sh` and
`configure-libs.sh`), and that is a decision for the maintainers. What it
does: a byte below `0x80` skips the UTF-8 decode in `dump_string()` and in
`utf8_check_string()`, and the lexer saves a run of plain ASCII of a string
token at once instead of a byte per callback. Every result, every byte and
every error stays the same: jansson's own 215 tests pass, and a differential
fuzz (original and patched library, the same driver, 120 000 random
documents, valid and invalid, loaded with three sets of flags and dumped with
five, half of them with the allocator capped to force "not enough memory")
gave the same bytes, the same errors and the same positions.

Instructions (`callgrind`), round 2 -> round 2 + the patch:

| Benchmark | Round 2 | + patch | Change |
|-----------|---------|---------|--------|
| `test_topic_pkey_integer` (appends) | 18.85 G | 12.38 G | **-34.3%** |
| `test_topic_pkey_integer_iterator2` (reads) | 11.21 G | 4.43 G | **-60.5%** |
| `perf_tr_treedb` | 25.46 G | 23.64 G | -7.1% |

Time (A/B, alternated, 8 rounds), round 2 -> round 2 + the patch:

| Benchmark | Round 2 | + patch | Change | t |
|-----------|---------|---------|--------|---|
| Appends/s, with a live reader (rt list) | 224 510 ± 20 305 | 279 321 ± 15 220 | **+24.4%** | 6.1 |
| Appends/s, without an rt list | 192 880 ± 16 023 | 241 592 ± 28 682 | **+25.3%** | 4.2 |
| Records read/s, iterator with callback (`..._iterator2`) | 193 384 ± 10 065 | 397 512 ± 48 633 | **+105.6%** | 11.6 |
| Pages read/s, forward (`..._iterator5`) | 177 328 ± 16 980 | 347 513 ± 22 316 | **+96.0%** | 17.2 |
| Pages read/s, backward | 183 059 ± 10 805 | 343 954 ± 38 723 | **+87.9%** | 11.3 |
| BFF logins (`perf_auth_bff`, K msg/s) | 7.97 ± 0.27 | 9.16 ± 0.53 | **+15.0%** | 5.7 |
| A page of 1 row, replica (`perf_timeranger2`, µs) | 6.88 ± 0.68 | 5.96 ± 0.25 | -13.4% | 3.6 |

The rest of `perf_timeranger2`, `perf_tr_treedb` and the TCP echo moved
inside their noise (|t| < 3), most of them in the same direction. A record
read is twice as fast: its cost was the parse, and the parse of ASCII is
now a copy.
