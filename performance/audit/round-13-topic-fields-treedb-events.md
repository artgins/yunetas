# Round 13: a topic's fixed fields read once, and the state of a treedb's events in C

Round 11 left two items for a decision: the static fields of a timeranger2
topic, read again by every append, and the treedb's events, whose hold,
publication and release looked up the same keys of the treedb on every
write. The user went ahead with both (2026-10-24).

Base: round 12 (`a8d3e37`), on the machine and with the method of the
[README](README.md).

## A: a topic's fixed fields, read once per topic (`c376f95`)

### What the profile showed

Every append (`tranger2_append_record()`) read from its topic's json the
fields that say what a record is: `system_flag`, `pkey` and `tkey`, and the
`filename_mask` that names its file (`get_file_id()`). Four lookups per
record, some 600 instructions of the 20 000 of an append in
`test_topic_pkey_integer`.

### The change

**A topic's fixed fields are read once for each topic json**
(`kernel/c/timeranger2/src/timeranger2.c`, `topic_fixed_of()`).

- **Why they can be kept.** They are the topic's from its open: a
  `topic_var` changes the topic's other fields, never these
  (`replace_topic_var()` updates the topic with `kw_update_except()` and
  leaves out `topic_fields`), and nothing else writes them.
- **Where they live.** In one of 64 slots, chosen by the pointer of the
  topic's json. A slot holds a reference to its topic, so that pointer
  cannot become another topic's while the slot keeps it: what the slot read
  is that topic's for as long as it is there. Two topics on one slot take
  turns, each one read again when it comes back.
- **When they go.** The close of a topic lets its slot go
  (`topic_fixed_forget()` in `tranger2_close_topic()`, and
  `tranger2_stop()` closes them all). The shutdown of a tranger lets go the
  topics that only a slot holds then (`topic_fixed_sweep()`).
- **What stays a lookup.** `master`: `revive_stopped_tranger()` changes it
  at run time. The tranger's own `filename_mask`, which `get_file_id()`
  takes when the topic has none.

## B: the state of a treedb's events, in a C structure (`ae6aa3c`)

### What the profile showed

A saved write holds the events of its treedb, publishes them and releases
them (`hold_treedb_events()`, `publish_treedb_event()`,
`release_treedb_events()`). Each step looked up again, in the tranger's
json, the treedb, its callback, its user data, `__events_held__` and
`__deferred_events__`, and the release walked the tranger's treedbs to find
the name of its own: 19 lookups per saved write, the three functions some
11% of the instructions of `perf_tr_treedb` (inclusive).

### The change

**A state of each open treedb (`treedb_state_t`) keeps those pointers and
values, read once from the json** (`kernel/c/timeranger2/src/tr_treedb.c`).

- **How it is found.** `treedb_state_of(tranger, name)` looks for it by its
  tranger and its name, or makes it at the first look, from the json. The
  hold, the publication, the release and `get_treedb()` read it; the
  release takes the treedb's name from it.
- **The json stays what says it.** The keys are still written in the
  treedb's json: a dump of the tranger shows them, and the tests compare
  it. The state follows the only places that write them:
  `treedb_set_callback()` reads the callback and its data again, and
  `__events_held__` and `__deferred_events__` are made once, by the first
  hold and the first held event, which keep them in the state too, and are
  then changed in place.
- **Why its treedb is its tranger's.** A state holds a reference to its
  tranger, so that pointer cannot become another tranger's while the state
  is kept. The json of a treedb is put in the tranger by `treedb_open_db()`
  and taken out only by `treedb_close_db()`, which lets the state go first.
  So while a state is kept, its treedb is the one of its tranger.
- **A tranger shut down without closing its treedbs** is held, then, by
  their states alone; they are let go, and the tranger with them, at the
  next look that finds no state or at the next close of a treedb. A first
  version held the treedb's json instead of the tranger and let a state go
  when only it held that json: the program that checks the references (below)
  showed that a state is then kept for as long as anybody else holds the
  treedb's json, and that while it is kept another tranger made at the same
  address would find it. Holding the tranger leaves no such case.
- **What stays as it was.** The colder readers of the callback (the create,
  the delete, `treedb_delete_instance()`, `set_volatil_values()`, the
  unlinks a delete tells at once) read the json as before.

What a caller can see of A and B: nothing. The same records, files, events,
in the same order, the same json in the tranger, the same logs and errors.

## How it was checked

- **The references**, by a program that holds references of its own to the
  jsons and reads their counts (valgrind cannot follow the allocations of a
  static binary):
    - a topic: after an append, held by the tranger, a slot and the
      program; after `tranger2_close_topic()`, and after
      `tranger2_shutdown()` of a reopened one, by the program alone;
    - a treedb: after an open, a callback and two writes (two events told),
      the tranger is held by its owner and the state, and the treedb's json
      by its tranger; after `treedb_close_db()`, the tranger by its owner
      alone;
    - a tranger shut down without the close: held by the state alone; kept
      while somebody else holds it too; a second tranger opening a treedb of
      the same name gets a state and a json of its own and writes to them;
      at the next close, the first tranger and its treedb are let go.
- **The timeranger2 and treedb tests** after each part, serially (95
  tests): the six that fail on this machine without the change fail with
  the same messages, the others pass.
- **The suite of the SDK** on the result (every yuno rebuilt), 319 tests:
  the 9 failures of this machine, each one with the same messages as in
  rounds 9 to 12, and nothing else.

## Measured

### Instructions (`callgrind`), round 12 -> round 13

| Benchmark | Round 12 | Round 13 | Change | A | B |
|-----------|----------|----------|--------|---|---|
| `perf_tr_treedb` (writes, links, deletes) | 11.84 G | 10.92 G | **-7.8%** | -1.1% | -6.8% |
| `test_topic_pkey_integer` (appends) | 7.54 G | 7.33 G | **-2.8%** | -2.8% | |
| `perf_timeranger2` (appends, cache, reads) | 28.39 G | 27.83 G | **-2.0%** | -2.0% | |
| `perf_c_treedb` (open of 40 treedbs of a dynamic schema) | 13.89 G | 13.79 G | -0.7% | -0.3% | -0.4% |
| `perf_tr_treedb_read` (open, lists, views) | | 6.85 G | | | -0.4% |

Round 12 did not change `timeranger2.c`: the figures of round 11 are its
own for the two benchmarks of timeranger2. `perf_tr_treedb_read` was not
measured in round 12; part B on its own takes it from 6.88 G to 6.85 G. A
blank is a benchmark that does not run the part: the benchmarks of
timeranger2 open no treedb.

In `perf_tr_treedb`, part B takes away 5.2 million of the 39.3 million
lookups of a key (`hashtable_get()`), and the hold, the publication and the
release go from 11.5% of the instructions to 5.4% (inclusive), most of what
is left being the callbacks they run. Finding a state costs some 56
instructions.

### Time (A/B, alternated), round 12 -> round 13

The two binaries in turn, the order turned every round, with a `sync` and a
2 s pause before each run: `perf_tr_treedb` and `test_topic_pkey_integer`
20 rounds, `perf_timeranger2` 10. Mean ± standard deviation; t is the change
over its standard error (|t| < 3 is noise).

| Case | Round 12 | Round 13 | Change, t |
|------|----------|----------|-----------|
| `perf_tr_treedb`, `update_memory` (µs of CPU) | 1.52 ± 0.38 | 1.33 ± 0.32 | -12.6%, -1.7 |
| `perf_tr_treedb`, `update_saved` (µs of CPU) | 6.08 ± 0.55 | 5.95 ± 0.88 | -2.1%, -0.6 |
| `perf_tr_treedb`, `link_unlink` (µs of CPU) | 6.85 ± 0.62 | 6.85 ± 1.07 | +0.1%, 0.0 |
| `perf_tr_treedb`, `delete_parent` (µs of CPU) | 3 524 ± 187 | 3 405 ± 139 | -3.4%, -2.3 |
| `perf_timeranger2`, `tm_build_appends` (µs) | 3.21 ± 0.38 | 2.95 ± 0.33 | -8.2%, -1.7 |
| `perf_timeranger2`, `build_appends` (µs) | 42.5 ± 5.8 | 41.7 ± 5.8 | -1.9%, -0.3 |
| `test_topic_pkey_integer`, appends/s | 271 799 ± 29 481 | 260 075 ± 30 342 | -4.3%, -1.2 |
| `test_topic_pkey_integer`, appends/s (second run) | 224 235 ± 29 662 | 231 439 ± 26 203 | +3.2%, 0.8 |

Every case moved within its noise, which on this VM was 10% to 25% of a
case; the largest moves (an update in memory, an append with a time key, a
delete of a parent) go the way of the instructions. A change of 3% to 8% of
the instructions does not show against that noise, and an append's time is
mostly its writes to disk, which `callgrind` does not count: the
instructions are the figure of this round. The other cases (the creates of
links, the forced deletes, the reopens, and the opens and reads of
`perf_timeranger2`, whose time is mostly outside an append) moved within
their noise as well, the largest t being 1.5.

## Found, not done

The lookups are still a third of a treedb write (`hashtable_get()` 32% of
`perf_tr_treedb`), but no reader of them is large any more. Who looks up
(inclusive shares of `perf_tr_treedb`, after this round): the path readers,
`kw_find_path()`, 4.6%; a node's metadata (`md_get_str()`, the
`__md_treedb__` of the node and then its key), 4.2%; the append of
timeranger2, 3.5%; the cache of a key (`get_cache_range()`), 2.7%;
`treedb_topic_pkey2s()`, 2.6%; `node_is_pure()`, 2.4%;
`treedb_get_id_index()`, 1.9%; `get_topic_wr_fd()`, 1.9%; and a long tail
under 1.6% each. Each one is the remedy of these rounds again (a value read
once and kept where its owner can say when it changes), for a gain of 1% to
3% each, and most of them reach jsons that callers hold and change (a
node's `__md_treedb__`, a key's cache), where a kept reading needs every
writer to say so. Not proposed: what is left is spread thin, and each
reading kept is one more invariant for every writer of that json.
