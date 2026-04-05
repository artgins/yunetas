# timeranger2 test

Low-level tests for the `timeranger2` store: topic create/open/close, record append, iterator (forward / reverse, by time range, by rowid), and on-disk md2 file consistency.

## Run

```bash
ctest -R test_timeranger2 --output-on-failure --test-dir build
```

See also `tr_msg` (message wrapper), `tr_queue` (queue semantics) and `tr_treedb` (graph db).
