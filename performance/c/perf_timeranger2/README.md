# perf_timeranger2

Benchmark of timeranger2 alone (no gclass, no yuno): the open of a store, the
create of topics, a tm query, and the read path of an iterator as a master and
as a replica.

| Case | What it measures |
|---|---|
| `build_appends` | The appends that build the store of `open`: 2000 keys x 10 daily md2 files x 20 rows. |
| `open_master`, `open_replica` | One open of that topic (20 000 md2 files), mean of 10, as a master and as a replica. |
| `create_topic` | 10 topics created (each one writes its topic files with their fsyncs). |
| `topic_version_change` | The `topic_version` of those 10 topics raised. |
| `tm_build_appends` | The appends of a topic of 1 key x 30 daily files x 20 000 rows. |
| `tm_query_unmigrated` | A tm query of one minute (`from_tm` / `to_tm`) in the middle file of that topic: a filter on its 600 000 rows. The name is the one it had while the tm markers existed (7.25.5..7.25.22), kept so the reports chart one trend; `mark_tm_order` and `tm_query_migrated` went with the markers. |
| `read_open_<role>` | The open and the close of an unfiltered iterator on a key of 10 000 rows (10 daily files x 1000 rows), mean of 2000. `<role>` is `master` or `replica`: the same store opened by the tranger that owns it and by one that follows it. |
| `read_open_filtered_<role>` | The same with a filter (`from_rowid`/`to_rowid`, 100 rows in the middle of the key): the open builds the iterator's index. |
| `read_page1_<role>` | `tranger2_iterator_get_page()` of 1 row of the unfiltered iterator, mean of 20 000: what one page costs beyond its rows. |
| `read_page1_filtered_<role>` | A page of 1 row of the filtered iterator (a position of its index), mean of 20 000. |
| `read_page100_<role>` | A page of 100 rows of the unfiltered iterator, mean of 1000. |

## Run

```bash
$YUNETAS_OUTPUTS/bin/perf_timeranger2            # every phase, the reference sizes
$YUNETAS_OUTPUTS/bin/perf_timeranger2 tm         # one phase: open, create, tm or read
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
+- standard deviation. Linked against 7.25.5..7.25.22, the topic the tm phase
creates marks its tm order, and `tm_query_unmigrated` is the query of a marked
topic (~7 ms, the files out of range skipped); linked against 7.25.4 it is the
query of 7.25.4 (~13 ms, wrong when a file's tm went back). The figures are in
`../README.md`.
