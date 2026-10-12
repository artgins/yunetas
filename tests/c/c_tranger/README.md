# c_tranger test

Tests the commands of the `C_TRANGER` GClass, the service that wraps a
timeranger2 store for the control plane. The test creates a `C_TRANGER` as
a master yuno, fills a topic through its own tranger handle, and drives every
command through `gobj_command()`, the path a `command-yuno` takes.

What it checks:

- **Reading** -- `topics` (names, or a desc each with `expanded=1`),
  `list-keys` (the record count and the time span of each key on both axes),
  `open-iterator` and `get-page` by `t` and by `tm` (two independent axes that
  combine), several keys in one iterator (`rkey`), a backward page that
  counts from the live end, and the frozen counts of a filtered multi-key
  iterator.
- **Handles** -- a handle of a topic that was closed is dropped, not
  dereferenced; a key deleted under an iterator; a keys watch that is not a
  client's feed; a `no_rt` list freed with its topic; a handle of one session
  refused to another (`-403`), and through the agent's one link a handle of
  one user refused to another user.
- **Writes** -- `add-record` asks `write`, answers `-403` on a refusal, and
  appends a dict or its json text; `delete-key` with `force`.
- **A peer's data** (phases *"a peer's data is refused"* and *"more of a
  peer's data is refused"*). Each case is sent with a `__username__` in the
  kw, as `C_IEVENT_SRV` delivers a peer's command, and each refusal is
  asserted as a WARNING (`__priority__`), with the side effects checked (the
  system topic is still there, no topic was made, no record was appended):
  - an unknown topic, a name with `/` or `..`, a system topic to delete;
  - `create-topic` of a wrong shape: an unknown `system_flag`, two key types,
    no `pkey`, `jn_cols` that is not a dict, a name longer than `NAME_MAX`, a
    topic named as a FILE of the database, a new `topic_version` over a
    system topic, over a topic of a treedb, or without `jn_cols` (its
    columns were written empty); a plain topic with its `jn_cols` still takes
    a new version, and a topic with no columns by design takes one with
    `jn_cols={}`;
  - a NEW topic named as what a store keeps (`.hidden`, `__x`, `x.json`,
    `x.bak`) and a `jn_var` of what the tranger keeps or types
    (`system_topic`, `last_rowid_id` `"x"`, `directory`, `topic_version`
    `"2"`);
  - a peer's write into a topic of a treedb (`add-record`, `delete-key`,
    `delete-topic`) and a new topic in its store, while a C caller's
    `add-record` there is taken;
  - `add-record` of a record that is not a dict, without its pkey, with a
    pkey that is a path, a `__t__` of `9223372036854775807` and of
    `TRANGER2_MAX_TIME + 1` (both refused as larger than the 44 bits of an
    md2 row: every `__t__` of 44 bits is a date), and an integer tkey time of
    more than 44 bits;
  - `add-record` of a tkey that is a DATE past the 44 bits: `"@99999999999999
    +0000"` on the plain topic, and `"@20000000000 +0000"` on a new
    `sf_tm_ms` topic (2e10 seconds are 2e13 milliseconds), both refused with
    nothing appended; a date that topic keeps (`"@1700000000 +0000"`) is
    appended. The check is the append's own computation,
    `tranger2_record_tkey_time()`;
  - a key that is a path (`open-iterator`, `open-list`, `open-rt`), a time
    bound that is a dict (and an integer one, taken);
  - `print-tranger` paths that `kw_find_path()` logged an ERROR for, and a
    string at the end of a path, answered as it is;
  - a one-shot `open-list` whose `list_id` is the name of the service reads
    a key that has an iterator open;
  - `check-json` asks `read`, and a `-403` names the yuno first;
  - the same refusal from C code (no `__username__`) is an ERROR.
- **A replica** -- a master that lost its lock reads as a replica, and
  `delete-key`, `delete-topic` and the `create-topic` of a new topic answer
  READ-ONLY with no ERROR.

The commands and their answers are documented in
[`docs/doc.yuneta.io/api/gclass/data.md`](../../../docs/doc.yuneta.io/api/gclass/data.md)
(`C_TRANGER`, *What a command is handed is the caller's data*).

## Run

```bash
ctest -R test_c_tranger --output-on-failure --test-dir build
```
