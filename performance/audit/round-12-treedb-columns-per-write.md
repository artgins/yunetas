# Round 12: the columns of a topic read once for a whole write

Round 11 left, for a decision, a topic's column reading handed down an
update: `topic_cols_get()` was asked five times per saved update, three
lookups each. The user went ahead with it (2026-10-24).

Base: round 11 (`5722065`), on the machine and with the method of the
[README](README.md).

## What the profile showed

One update of a node asked the columns of its topic in five functions,
one after the other: `treedb_store_files()`, `prepare_node_update()`,
`keep_node_fkeys()`, `link_file_columns()`, and `convert_node2tranger()`
in the save. Each ask found round 10's reading at once, but finding it
takes three lookups (the topics of the tranger, the topic, its `cols`):
1.03 million asks in `perf_tr_treedb`, 3.8% of its instructions.

## The change (one commit, `a75411d`)

**An operation reads the columns of its topic once and keeps them in a
slot** (`kernel/c/timeranger2/src/tr_treedb.c`).

- **The slot** (`topic_cols_of_op()`): the first function of an operation
  that asks reads the columns (`topic_cols_get()`) and leaves them there;
  the next ones take them from it, some 50 instructions. The slot answers
  only for the topic it was read of, compared by name: any other topic, a
  NULL name, or no slot is asked of `topic_cols_get()` as before.
- **Who has one.** A write of a node carries it in its `node_write_t`, and
  gives it back at its close. The update, the autolink, the replace of
  links and the update with links fill it before the write opens (in
  `prepare_node_update()` and the files' store) and hand it to their write.
  A create holds its own: `treedb_create_node()` is now a wrapper around
  `create_node()`, which holds the body.
- **How long it is held.** The columns of a topic change only at an open,
  a create-topic or a delete-topic, never inside a write. The slot is
  given back before the append of the record (`append_node_record()`, and
  the create's), which runs the callbacks of the topic's lists; nothing of a
  write reads the columns after it. So no reading of an operation is held
  while a callback runs, as before. In the create, what asks after its own
  callback (its 'file' columns) reads them again.
- **The public functions keep their signature.** `treedb_save_node()` and
  `treedb_store_files()` call `save_node()` and `store_files()`, which take
  the slot; their bodies moved there, and their logs keep naming the public
  function, as the create's do.

What a caller can see: nothing. The same answers, records, events, logs and
errors; a stack trace printed in an error case has the wrapper as one more
frame.

### How it was checked

- **The timeranger2 and treedb tests**, serially (88 tests): the six that
  fail on this machine without the change fail with the same messages, the
  others pass, and none logs a reading left taken at the close of the last
  treedb (what `topic_cols_sweep()` says of a slot never given back).
- **The suite of the SDK** on the result (every yuno rebuilt), 319 tests:
  the 9 failures of this machine, each one with the same messages as in
  rounds 9 to 11, and nothing else.

## Measured

Asks of the columns in `perf_tr_treedb`: 1.03 million -> 321 000, the first
of each operation (the files' store in an update or a create, the save of a
link or an unlink).

| Benchmark (`callgrind`) | Round 11 | Round 12 | Change |
|-------------------------|----------|----------|--------|
| `perf_tr_treedb` (writes, links, deletes) | 12.08 G | 11.84 G | **-2.0%** |
| `perf_c_treedb` (open of 40 treedbs of a dynamic schema) | 13.89 G | 13.89 G | 0.0% |

In time (A/B, alternated, 20 rounds of `perf_tr_treedb`, CPU time per
operation), every case moved within its noise, which on this VM was 10% to
20% of a case: a saved update -4.7% (t -1.4), an update in memory +1.7%
(t 0.3), a link and an unlink +1.7% (t 0.4). A change of 2% of the
instructions does not show against that; the instructions are the figure
of this round.

## Not done

What round 11 left for a decision and this round did not take stays as it
was written there: the treedb's events in a C structure (about 9% of a
write), and a timeranger2 topic's static fields read once (some 16 lookups
per append). Both were done in
[round 13](round-13-topic-fields-treedb-events.md).
