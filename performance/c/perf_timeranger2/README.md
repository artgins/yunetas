# perf_timeranger2

Benchmark of timeranger2 alone (no gclass, no yuno): the open of a store, the
create of topics, and a tm query before and after `tranger2_mark_tm_order()`.

| Case | What it measures |
|---|---|
| `build_appends` | The appends that build the store of `open`: 2000 keys x 10 daily md2 files x 20 rows. |
| `open_master`, `open_replica` | One open of that topic (20 000 md2 files), mean of 10, as a master and as a replica. |
| `create_topic` | 10 topics created (each one writes its topic files with their fsyncs). |
| `topic_version_change` | The `topic_version` of those 10 topics raised. |
| `tm_build_appends` | The appends of a topic of 1 key x 30 daily files x 20 000 rows. |
| `tm_query_unmigrated` | A tm query of one minute (`from_tm` / `to_tm`) in the middle file of that topic, made to look like a topic of 7.25.4 or earlier (no `marks_tm_unordered`). |
| `mark_tm_order` | The migration of that topic (`tranger2_mark_tm_order()`). |
| `tm_query_migrated` | The same query after the migration. |

## Run

```bash
$YUNETAS_OUTPUTS/bin/perf_timeranger2            # every phase, the reference sizes
$YUNETAS_OUTPUTS/bin/perf_timeranger2 tm         # one phase: open, create or tm
$YUNETAS_OUTPUTS/bin/perf_timeranger2 --small    # sizes / 10: what ctest runs
```

Every result is one line of JSON on stdout, e.g.:

```json
{"bench": "perf_timeranger2", "case": "open_master", "seconds": 0.082630, "ops": 1, "us_per_op": 82630.000, "keys": 2000, "md2_files": 20000}
```

The first line gives `yuneta_version`, `track_memory` and the sizes. The
store is `~/tests_yuneta/perf_timeranger2`, removed at the end.

## Against another release

Compile the other release's `timeranger2.c` and put its object before the
libraries in the link line of the benchmark (the one in
`build/CMakeFiles/perf_timeranger2.dir/link.txt`). Run the two binaries
alternated, 10 rounds or more, with a `sync` and a pause before each run (a
run leaves much to write back, and it slows the next one), and compare mean
+- standard deviation. Linked against 7.25.4,
`tranger2_mark_tm_order()` does not exist (the benchmark declares it weak):
the tm phase then prints `mark_tm_order_not_linked`, and
`tm_query_unmigrated` is the tm query of 7.25.4. The figures are in
`../README.md`.
