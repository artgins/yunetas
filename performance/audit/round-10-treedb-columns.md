# Round 10: the columns of a treedb topic, read once

Round 9 left one large cost in the treedb: its lookups of keys. In
`perf_tr_treedb`, `hashtable_get()`, `hash_str()`, `json_object_get()` and
`strlen()` were 38% of the instructions, and jansson's lookup was already
short; what cost was how many of them Yuneta made. The user went ahead with
it (2026-10-24). This round takes away the largest group: the columns of a
topic, read again for every node.

Base: round 9 (`linux-ext-libs` 1.33, jansson `20fd432`), on the machine
and with the method of the [README](README.md).

## What the profile showed

To write or to view a node, the treedb walks the columns of its topic
(`tranger["topics"][topic]["cols"]`, a dict of column descriptions), and asks
each column, at every node, the same questions:

- **Its flags**, with `kw_has_word()`: is the column a `hook`, a `fkey`,
  `persistent`, `required`, `now`, `enum`, `wild`, `time`, `template`,
  `notnull`, `file`, `rowid`, `hidden`? Each question is a walk of the list
  of flags of the column, with a `strstr()` of the word in each item. A write
  asks up to fourteen per column. In round 9, `kw_has_word()` was
  **8.1%** of the instructions of `perf_tr_treedb` (1.23 G of 15.25 G), and
  `get_col_view_flags()` (the five flags of a view, in one walk since round
  5) **10.1%** of `perf_tr_treedb_read` (0.87 G of 8.60 G).
- **Its `type`**, with `kw_get_str(col, "type")`: a path read for every
  column of every node written.
- **The columns themselves**, with `tranger2_dict_topic_desc_cols()`, 3.5%
  of `perf_tr_treedb` and 1.7% of `perf_tr_treedb_read`: four or five times
  for a write of a node.

The answers are the same every time. A column's description is never
changed in place: when the columns of a topic change, its `cols` is replaced
by another json (at an open, at a create-topic, at a new `topic_version`,
in `tranger2_write_topic_cols()`), and `parse_hooks()` writes in place only
their `fkey` marks, which are not among the questions. One function changes a
`cols` in place: `derive_file_hooks()`, which adds and removes on `__assets__`
the hooks it derives from the file columns of the other topics.

## The change (one commit)

**The columns of a topic are read once, while its `cols` is the same json**
(`kernel/c/timeranger2/src/tr_treedb.c`). A reading (`topic_cols_t`) holds,
for each column in the order of the `cols`, its name, its description, its
`type` (when it is a string) and its flags as bits: the 13 words the treedb
asks, each one what `kw_has_word(flag, word, 0)` answers (a substring of an
item of a list, a true key of a dict, a substring of a string), so the same
answers by construction.

- **Where it lives.** A registry of the readings of the open treedbs, found
  by the pointer of the topic's `cols`. The reading holds a reference to that
  json, so its pointer cannot become another json's while the reading is
  kept. When the `cols` of a topic is replaced, the next ask finds no reading
  for the new pointer and makes one; the old one is freed when nobody uses it
  and no topic holds its `cols` any more (a sweep at each new reading and at
  each close of a treedb). The last close of a treedb frees them all, as it
  does `topic_cols_desc`.
- **The one change in place.** `derive_file_hooks()` says so
  (`topic_cols_forget()`) after it adds or removes a hook; and a reading
  whose number of columns is not that of its `cols` any more is made again.
- **Who reads it.** The write of a node (`create_pure_record()`,
  `convert_node2tranger()`, `prepare_node_update()`, `keep_node_fkeys()`,
  `treedb_store_files()`, `link_file_columns()`) and its view
  (`node_collapsed_view()`). `normalize_node_field_value()` takes the
  column's `type` and flags from its caller; its other two callers
  (`set_create_key2_values()` and `create_template_record()`, rare) pass
  none, and it reads them from the column as before.
- **What stays.** When a column's `type` is not a string, the reading has
  none and the code reads it with `kw_get_str()` as before, so a
  description without a `type` is logged as it was. When the columns of a
  topic cannot be read, the reading is not made and the same error is
  logged, by the same call. With no treedb open, a reading is used and freed
  at once.

What a caller can see: nothing. The same results, records, events, logs and
errors.

### How it was checked

- **The treedb tests**, serially (the treedb, `C_TREEDB`, `C_NODE`,
  `C_AUTHZ`, msg2db and tranger tests, 41): the six of them that fail on
  this machine without the change fail with the same messages, and the
  others pass. A first version did not know that `derive_file_hooks()`
  changes the `cols` of `__assets__` in place, and `test_tr_treedb_files`
  caught it ("hook field not found"): hence `topic_cols_forget()` and the
  check of the number of columns.
- **The suite of the SDK** on the result (every yuno rebuilt), 319 tests:
  the 9 failures of this machine, each one with the same messages as in
  round 9, and nothing else.

### What was not changed

- **The filter of a list** (`match_node_simple()`) keeps its one walk of the
  flags (`get_col_view_flags()`): it is handed the topic's `cols` json by
  `treedb_list_nodes()` and `treedb_list_instances()`, whose `match_fn` the
  caller may replace with its own, of the same signature. Its walk of the
  flags is some 0.6% of `perf_tr_treedb_read`.
- **The load of a treedb** (`load_id_callback()`, `convert_tranger2record()`,
  `load_links()`): it asks a column's flags once per node loaded, at the
  open; with its reopens, under 1% of the instructions of
  `perf_tr_treedb_read`.

## Measured

### Instructions (`callgrind`), round 9 -> round 10

| Benchmark | Round 9 | Round 10 | Change |
|-----------|---------|----------|--------|
| `perf_tr_treedb` (writes, links, deletes) | 15.25 G | 12.81 G | **-16.0%** |
| `perf_tr_treedb_read` (open, lists, views) | 8.60 G | 7.00 G | **-18.7%** |
| `perf_c_treedb` (open of 40 treedbs of a dynamic schema) | 16.84 G | 14.00 G | **-16.9%** |
| `perf_auth_bff` (per login) | 638 830 | 638 678 | 0.0% |

`kw_has_word()` went from 8.1% of `perf_tr_treedb` to 0.2%, and
`get_col_view_flags()` (10.1% of `perf_tr_treedb_read`) is left to the
filter, some 0.6% of it. The open of a treedb of a dynamic schema gains as
much as a write because it is writes: the system schema keeps each column of
each topic as a node, created through the same path. A BFF login does not
reach the treedb's write or view. The other benchmarks (`perf_ievent`,
`perf_mqtt`, `perf_tcp_test4`, `perf_timeranger2` and the two tests of
timeranger2) do not run this code.

### Time (A/B, alternated), round 9 -> round 10

The two binaries in turn, the order turned every round, with a `sync` and a
2 s pause before each run, 10 rounds; `perf_tr_treedb_read` 16 more rounds
of its own, whose figures are these. CPU time per operation (µs), mean ±
standard deviation; t is the change over its standard error (|t| < 3 is
noise).

| Case | Round 9 | Round 10 | Change, t |
|------|---------|----------|-----------|
| `perf_tr_treedb`, `update_memory` | 2.24 ± 0.27 | 1.42 ± 0.06 | **-36.4%**, -9.5 |
| `perf_tr_treedb`, `update_saved` | 7.46 ± 0.35 | 5.91 ± 0.21 | **-20.7%**, -12.0 |
| `perf_tr_treedb`, `link_unlink` | 7.05 ± 0.43 | 6.52 ± 0.29 | **-7.5%**, -3.2 |
| `perf_tr_treedb_read`, `get_view` | 4.23 ± 0.44 | 3.51 ± 0.32 | **-17.0%**, -5.3 |
| `perf_tr_treedb_read`, `view_users` | 6.45 ± 0.38 | 5.47 ± 0.27 | **-15.2%**, -8.5 |
| `perf_tr_treedb_read`, `view_departments` | 130.3 ± 12.6 | 123.3 ± 12.7 | -5.4%, -1.6 |
| `perf_c_treedb`, `seed` (wall, ms per treedb) | 327 ± 32 | 322 ± 23 | -1.4%, -0.4 |
| `perf_c_treedb`, `newer_literal` (wall, ms) | 93.2 ± 8.7 | 90.5 ± 9.6 | -2.9%, -0.7 |

An update of a node in memory takes a third less, and a saved one a fifth
less; a view, a sixth. `perf_c_treedb` measures wall time, and the open of a
treedb is mostly the disk: its 17% fewer instructions are within the noise
of its time. The other cases moved within their noise: the lists, the
reopens, the deletes, and `dump_views`, whose timed part (the dump of views
built before) does not run this code; the 10 rounds gave it +12% (t 1.8),
the 16 rounds -0.3%.

## Found, not done

- **The lookups are still half of a write.** In `perf_tr_treedb`,
  `hashtable_get()`, `hash_str()`, `json_object_get()` and
  `json_object_getn()`, with their `memcmp()` and `strlen()`, are some 50%
  of the instructions after this round. jansson's lookup is short; what
  costs is that the same keys are read again and again. Who reads them
  (inclusive shares of `perf_tr_treedb`):
    - **Path reads**, `kw_find_path()`: 8.3 M of them, 14.7%. 2.9 M come
      from `kw_get_str()`, most of them a node's `__md_treedb__` fields
      (`topic_name`, `treedb_name`, ...) read again by each function of one
      write; the walk of a path itself is some 50 instructions, so what a
      faster `kw_find_path()` would gain is about 1%; the gain is in fewer
      reads.
    - **The append of timeranger2**, `tranger2_append_record()`: 17
      lookups of its own state per record, 3.9%.
    - **The treedb's events**: their publication, hold and release
      (`publish_treedb_event()`, `hold_treedb_events()`,
      `release_treedb_events()`), 3.0%, 1.9% and 2.3% in lookups.
    - **`node_is_pure()`** (2.1%) and **`treedb_get_id_index()`** (1.6%).
    - **This round's own ask**, `topic_cols_get()`: 3.8%, five asks per
      update of a node. A reading taken once per operation and handed down
      to its helpers would save most of it.

  Each of them is the same remedy as this round's: a value read once where
  it is first needed and handed down, or kept in a C structure of its
  owner. A round on the treedb's write path.
