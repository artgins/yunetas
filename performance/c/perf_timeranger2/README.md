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
| `tm_query_unmigrated` | A tm query of one minute (`from_tm` / `to_tm`) in the middle file of that topic: a filter on its 600 000 rows. The name is the one it had while the tm markers existed (7.25.5..7.25.22), kept so the reports chart one trend. `mark_tm_order` and `tm_query_migrated` went with the markers. |
| `append_rt_no_rkey` | 200 000 appends over 100 keys with one keyless `rt_mem` list open, with no `rkey`. Each case has a topic of its own, and its keys are made before the timed part. The records the list got (`records_seen`) are checked: a wrong count fails the case. |
| `append_rt_rkey` | The same with the `rkey` `^0+1$`: one key of the 100 goes to the list. |
| `append_rt_rkey_costly` | 200 appends over 20 keys of 20 digits with the anchored `rkey` `(*NO_JIT)^(0+)+$`, which reaches the limit of one match on 19 of them. |
| `first_append_rt_rkey_unanchored` | The first append of 200 new keys of 255 bytes with a list whose `rkey` is unanchored and costly at every position (`RK_UNANCHORED`): what a writer pays for a new key, once for each list. |
| `load_rt_rkey_unanchored` | The load of a live list (`tranger2_open_list()`) over 3000 keys of 36 bytes with that `rkey`: its time, and `refused` 1 when the budget of the load refused it. |
| `read_open_<role>` | The open and the close of an unfiltered iterator on a key of 10 000 rows (10 daily files x 1000 rows), mean of 2000. `<role>` is `master` or `replica`: the same store opened by the tranger that owns it and by one that follows it. |
| `read_open_filtered_<role>` | The same with a filter (`from_rowid`/`to_rowid`, 100 rows in the middle of the key): the open builds the iterator's index. |
| `read_page1_<role>` | `tranger2_iterator_get_page()` of 1 row of the unfiltered iterator, mean of 20 000: what one page costs beyond its rows. |
| `read_page1_filtered_<role>` | A page of 1 row of the filtered iterator (a position of its index), mean of 20 000. |
| `read_page100_<role>` | A page of 100 rows of the unfiltered iterator, mean of 1000. |

## Run

```bash
$YUNETAS_OUTPUTS/bin/perf_timeranger2            # every phase, the reference sizes
$YUNETAS_OUTPUTS/bin/perf_timeranger2 tm         # one phase: open, create, tm, read or rkey
$YUNETAS_OUTPUTS/bin/perf_timeranger2 --small    # sizes / 10: what ctest runs
```

Every result is one line of JSON on stdout, for example:

```json
{"bench": "perf_timeranger2", "case": "open_master", "seconds": 0.082630, "ops": 1, "us_per_op": 82630.000, "keys": 2000, "md2_files": 20000}
```

The first line gives `yuneta_version`, `track_memory` and the sizes, the `rk_*` ones too. The logs go to stderr, so stdout has only the JSON lines. The
store is `~/tests_yuneta/perf_timeranger2`, removed at the end.

## Against another release

Compile the other release's `timeranger2.c` and put its object before the
libraries in the link line of the benchmark (the one in
`build/CMakeFiles/perf_timeranger2.dir/link.txt`). Run the two binaries
alternated, 10 rounds or more, with a `sync` and a pause before each run (a
run leaves much to write back, and it slows the next one), and compare mean
+- standard deviation. Linked against 7.25.5..7.25.22, the topic the tm phase
creates marks its tm order, and `tm_query_unmigrated` is the query of a marked
topic (~7 ms, the files out of range skipped). Linked against 7.25.4 it is the
query of 7.25.4 (~13 ms, wrong when a file's tm went back). The figures are in
`../README.md`.

## The worst measured cost of an `rkey`

The time of one judgement of a key has no bound of its own but the compiled
code of the pattern (64 KB at most): a search reads no clock inside a start
position, and the first search of a key reads none at all. So the cost is
measured, with the worst patterns found. The figures of R102 (64 KB of code),
on the development machine, idle:

| What | Pattern | Worst measured |
|---|---|---|
| The first append of a new key of 255 bytes, a live list open: its first search, 7 frames at each of 256 positions | `(*NO_JIT)(?:h?\|i?\|j?\|k?\|l?\|m?\|n?)(?:~?~?~?~?~?~?~?~?){2048}(?!)` over keys with none of those letters | 146 ms (143 to 146, 3 runs of 200 keys) |
| The same, the key judged position by position: one position of 1000 frames | `(*NO_JIT)()(?:[\s\S]?){10}(?:[\s\S]?\|[\s\S]?)(?:\1\1\1\1\1\1\1\1){2048}(?!)\|$` | 129 ms (123 to 129) |
| The load of a live list over 3000 keys, with either pattern | the same | refused at 2.00 to 2.12 s |

With 1 KB of code (R101) the worst measured was 2.5 ms a key. To measure a
pattern of your own:

```bash
$YUNETAS_OUTPUTS/bin/perf_timeranger2 '--rkey=(*NO_JIT)(?:h?|i?|j?|k?|l?|m?|n?)(?:~?~?~?~?~?~?~?~?){2048}(?!)' rkey
```

The two unanchored cases then use it: `first_append_rt_rkey_unanchored`
gives the time of the first append of a new key, and `load_rt_rkey_unanchored`
the time of the load.
