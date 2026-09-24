# perf_c_treedb

Benchmark of the open of a dynamic-schema treedb (`impose_c_schema` off) by
`C_TREEDB`, in a store of many treedbs. A yuno with one service,
`C_PERF_TREEDB_OPEN`, drives `C_TREEDB` through its `open-treedb` and
`close-treedb` commands.

The store holds `treedbs` treedbs (40) of `topics` topics (10) of `cols`
string columns (20) and an id. One phase per timeout, every treedb opened
and closed:

| Case | What it measures |
|---|---|
| `seed` | The first open of each treedb with the literal 1: its first projection into `__system__`. |
| `newer_literal` | The open with the literal 2, where every header changed: a projection that rewrites 211 nodes of `__system__` per treedb. |
| `same_literal` | The open with the literal 2 again: nothing to project. |

## Run

```bash
$YUNETAS_OUTPUTS/bin/perf_c_treedb                                  # 40 treedbs, the reference size
$YUNETAS_OUTPUTS/bin/perf_c_treedb --config-file=small.json         # 4 treedbs: what ctest runs
```

Another size goes in a config file, like `small.json`:

```json
{
    "global": {
        "C_PERF_TREEDB_OPEN.treedbs": 4
    }
}
```

Every result is one line of JSON on stdout, e.g.:

```json
{"bench": "perf_c_treedb", "case": "same_literal", "seconds": 0.740000, "ops": 40, "us_per_op": 18500.000, "treedbs": 40, "topics": 10, "cols": 20}
```

The first line gives `yuneta_version` and `track_memory`. The store is
`~/tests_yuneta/perf_c_treedb`, removed at the end. An ERROR in the log (an
open that failed) makes it exit -1.

## Against another release

Compile the other release's `c_treedb.c` (with the headers of
`kernel/c/root-linux/src`) and put its object before the libraries in the
link line of the benchmark (`build/CMakeFiles/perf_c_treedb.dir/link.txt`).
The binary must keep the name `perf_c_treedb` (the yuno role), so put each
variant in its own directory, and link the object of this release the same
way, so both are built alike. Run them alternated, 8 rounds or
more, and compare the mean and the spread: the seed and the newer literal
are dominated by fsyncs and vary by a few percent from round to round. The
figures, and where an open spends its time, are in `../README.md`.
