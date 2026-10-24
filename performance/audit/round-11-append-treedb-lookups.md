# Round 11: fewer lookups in timeranger2's append and in the treedb's writes

Round 10 read a topic's columns once and left the lookups of keys still half
of a treedb write. The user went ahead with the round on them (2026-10-24).
This round takes away the ones that a local change can take away, with the
same answers, files, logs and errors, and says what is left and what it
would take.

Base: round 10 (`71846f5`), on the machine and with the method of the
[README](README.md).

## What the profile showed

In `perf_tr_treedb` after round 10, `hashtable_get()`, `hash_str()`,
`json_object_get()`, `json_object_getn()`, `memcmp()` and `strlen()` were
some half of the instructions: about 36 million lookups of a key, some 180
instructions each. jansson's lookup is short (a hash of four mixes for a key
of ten bytes); what costs is how many are made. They come from:

- **timeranger2's append**, a third of a treedb write and nearly all of
  `perf_timeranger2` and `test_topic_pkey_integer`: some 16 lookups of its
  own state per record, plus its helpers. Two of them stand out:
    - **`get_file_id()`**: the file of a record is named by `strftime()` of
      the `gmtime()` of its time, some 1 200 instructions per append (5.4% of
      `test_topic_pkey_integer`, 3.7% of `perf_timeranger2`), for a name
      that changes once a day with the default mask, `%Y-%m-%d`.
    - **The key's cache**: an append updates the cell of its file (its
      times and rows) and then the key's totals, which looked the four times
      of the cell up again, and then its global rowid, which looked up the
      cell's rows again.
- **The path readers of the treedb**: a node's metadata
  (`"__md_treedb__`topic_name"`, `treedb_name`, `pure_node`, `immutable`),
  2.9 million path reads, each one walking and copying its path.
- **`treedb_topic_pkey2s()`**: two or three times per update of a node, some
  820 instructions each (two path readers, and the tranger's gobj looked up
  for the logs).
- **The treedb's events** (hold, publish, release: 19 lookups per saved
  write), **`topic_cols_get()`** (five asks per saved update, three lookups
  each), `node_is_pure()`, `node_is_indexed()`, `treedb_get_id_index()`.

## Changes applied (one commit each)

1. **The file id of the last time, kept for every time of its unit**
   (`4c10ba8`, `timeranger2.c`). The last id made is kept with its mask and
   the unit of time of the finest field of the mask (`mask_time_unit()`),
   and answers again for the same mask and a time of the same unit. Only
   masks whose conversions are numeric and without a modifier are kept, the
   ones of `tranger2_mask_sorts_as_time()`: `Y G F m j d V u H R M T S`.
   With them `strftime()` writes fields of the `gmtime()` of the time alone,
   in no locale and no zone, and those fields are the same for every second
   of the unit: in UTC a second, a minute, an hour and a day begin at a
   multiple of 1, 60, 3600 and 86400 seconds of the epoch, and a week, a
   month and a year begin with a day. Any other mask (a name, `%s`, `%E`,
   `%O`, ...) is made every time, as before. A kept id that does not fit the
   caller's buffer is made again, so it fails as before. Checked by a
   program that compares `strftime(gmtime())` at the first, the last and a
   random second of 200 000 units of each of 12 accepted masks, up to the
   44 bits of an md2 time, in the C and the `es_ES` locale: none differs.
2. **The totals of a key from the cell the append just wrote** (`67062cd`,
   `timeranger2.c`). `update_cache_cell()` hands back what the cell holds
   after it, read of the integers `put_cache_int()` leaves pointed to: each
   one what the cell holds at its key, or NULL when it could not be written,
   and then a lookup gives 0 as well. The totals and the global rowid take
   those values: five lookups less per append. The load from disk reads the
   cell as before.
3. **A node's metadata read directly where the path reader would answer it**
   (`039526c`, `tr_treedb.c`). `md_get_str()` and `md_get_bool()` take the
   arguments of `kw_get_str()` and `kw_get_bool()` with the key, and read the
   value directly when the reader's answer is just that value: a string, or
   a json boolean, without `KW_EXTRACT`. In any other case they call the
   reader with the same path, which answers and logs as it always did. The
   75 reads of that shape are replaced.
4. **A topic's pkey2s read directly** (`88e7eb8`, `tr_treedb.c`). With no
   flag and no default, the two readers answer the topic's list of pkey2s
   or NULL, and log only when the tranger is not a dict. When the tranger
   and its topics are dicts that answer is read directly; any other case
   goes to the readers as before.

What a caller can see: nothing. The same files, records, rowids, answers,
logs and errors. One detail: the stack trace a path reader prints in an
error case has the helper of change 3 as one more frame.

### How they were checked

- **The timeranger2 and treedb tests** after each change, serially (86
  tests): the six that fail on this machine without the change fail with
  the same messages, and the others pass.
- **The suite of the SDK** on the result (every yuno rebuilt), 319 tests:
  the 9 failures of this machine, each one with the same messages as in
  rounds 9 and 10, and nothing else. A first run failed a tenth,
  `helpers/test_rotatory`, because the disk of this session had filled up
  with the binaries of earlier rounds (`rotatory()` stops below 1% free);
  with the space back it passes, and the whole suite was run again.

## Measured

### Instructions (`callgrind`), round 10 -> round 11

| Benchmark | Round 10 | Round 11 | Change |
|-----------|----------|----------|--------|
| `test_topic_pkey_integer` (appends) | 8.16 G | 7.54 G | **-7.6%** |
| `perf_tr_treedb` (writes, links, deletes) | 12.81 G | 12.08 G | **-5.7%** |
| `perf_timeranger2` (appends, cache, reads) | 29.92 G | 28.39 G | **-5.1%** |
| `perf_tr_treedb_read` (open, lists, views) | 7.00 G | 6.89 G | -1.5% |
| `perf_c_treedb` (open of 40 treedbs of a dynamic schema) | 14.00 G | 13.89 G | -0.7% |
| `test_topic_pkey_integer_iterator2` (reads) | 3.22 G | 3.22 G | +0.1% |

Each change on its own, as it was committed:

| Change | `test_topic_pkey_integer` | `perf_tr_treedb` | `perf_tr_treedb_read` |
|--------|---------------------------|------------------|-----------------------|
| 1. the file id kept | -4.4% | -1.6% | |
| 2. the totals from the cell | -3.2% | -0.7% | |
| 3. the metadata read directly | | -1.0% | -1.4% |
| 4. the pkey2s read directly | | -2.5% | -0.1% |

A blank was not measured on its own: changes 3 and 4 are the treedb's, which
`test_topic_pkey_integer` does not use, and the reads were measured after
changes 3 and 4 only.

The other benchmarks (`perf_ievent`, `perf_mqtt`, `perf_tcp_test4`,
`perf_auth_bff`) do not append and do not reach the treedb's write.

### Time (A/B, alternated), round 10 -> round 11

The two binaries in turn, the order turned every round, with a `sync` and a
2 s pause before each run: `perf_tr_treedb` and `test_topic_pkey_integer`
20 rounds, `perf_timeranger2` 10. Mean ± standard deviation; t is the change
over its standard error (|t| < 3 is noise). This VM was noisier than in
round 10: the deviations of the same cases were two to six times larger.

| Case | Round 10 | Round 11 | Change, t |
|------|----------|----------|-----------|
| `perf_tr_treedb`, `update_memory` (µs of CPU) | 1.59 ± 0.20 | 1.40 ± 0.19 | **-11.9%**, -3.1 |
| `perf_tr_treedb`, `reopen` (µs of CPU) | 36.6 ± 7.0 | 31.5 ± 2.3 | **-14.0%**, -3.1 |
| `perf_tr_treedb`, `delete_parent` (µs of CPU) | 3 687 ± 241 | 3 511 ± 204 | -4.8%, -2.5 |
| `perf_tr_treedb`, `update_saved` (µs of CPU) | 6.37 ± 0.54 | 6.35 ± 0.72 | -0.5%, -0.1 |
| `perf_timeranger2`, `tm_build_appends` (µs) | 3.25 ± 0.22 | 2.92 ± 0.18 | **-10.1%**, -3.7 |
| `test_topic_pkey_integer`, appends/s | 264 506 ± 28 830 | 275 111 ± 23 949 | +4.0%, 1.3 |
| `test_topic_pkey_integer`, appends/s (second run) | 221 853 ± 22 708 | 231 796 ± 18 131 | +4.5%, 1.5 |

The direction is that of the instructions, and where the noise lets it
show, an update in memory, a reopen and an append with a time key are a
tenth faster. A saved update does not move: its time is mostly the writes
to disk, which `callgrind` does not count. The other cases of
`perf_timeranger2` moved within their noise: its reads, which do not run
the changed code (up to +14%, t 1.5, with a deviation of 24%), and its
opens, which reach only the new argument of `update_cache_cell()` (the
largest t, 2.5, is the open of a replica, +6.6%).

## Not done, for a decision

What is left of the lookups is no longer a local change. Each item says
what it would take and what it would give, in `perf_tr_treedb` unless said
otherwise.

- **The treedb's events: about 9%.** A saved write holds its treedb's
  events, publishes them and releases them, 19 lookups: the treedb in the
  tranger, its callback, its user data, `__events_held__`,
  `__deferred_events__`, the topic's own name. They are keys of the treedb
  in the tranger's json, written by `treedb_set_callback()`, by the hold and
  by the first publication. Kept in a C structure of each treedb, they would
  cost nothing; but a dump of the tranger shows them, so either they stay
  in the json as well and every place that writes them must say so (the
  pattern of round 10, with `derive_file_hooks()` as its only writer in
  place), or the dump changes.
- **The static fields of a timeranger2 topic: some 16 lookups per append.**
  `system_flag`, `pkey`, `tkey`, `filename_mask`, `directory`, the topic in
  the tranger: read once per topic as round 10 read the columns, they would
  need the same knowledge of every place that changes a topic's json.
- **A topic's column reading handed down an update: about 3%.**
  `topic_cols_get()` is asked five times per saved update, three lookups
  each. Taken once and handed to `prepare_node_update()`,
  `treedb_store_files()`, `keep_node_fkeys()`, `link_file_columns()` and
  `convert_node2tranger()`, it would be asked once; that changes the
  signature of five private functions and needs private versions of two
  public ones (`treedb_save_node()`, `treedb_store_files()`).
- **`get_topic_wr_fd()`, twice per append**, looks up the key's dict of open
  files twice. Kept between the two calls it could be stale: the first one
  may close every open file (`close_fd_wr_files()` on `EMFILE`) and make
  the key's dict again. Not worth two lookups.
- **jansson's lookup itself**, some 180 instructions: `strlen()`, a hash of
  four mixes for a key of ten bytes, the bucket and a `memcmp()`. Little to
  take from it without a weaker hash.
