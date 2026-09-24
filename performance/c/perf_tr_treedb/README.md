# perf_tr_treedb

Benchmark of the treedb writes: `tr_treedb` over timeranger2, without the
`C_TREEDB` / `C_NODE` gclasses. One treedb of two topics, `users` and
`departments`, linked by a list fkey (`users.departments` ->
`departments.users`). A link callback is set, as `C_NODE` does, and the
events it gets are counted.

| Case | What it measures |
|---|---|
| `update_memory` | N updates of one node, not saved (`treedb_update_node(..., FALSE)`). |
| `update_saved` | N updates of one node, saved. |
| `link_unlink` | N/2 links and N/2 unlinks of one user to a department (each one saves the child). |
| `create_link_half` | N/50 creates, half of them linked. |
| `reopen` | The open of the treedb: every node and its links. |
| `delete_force` | N/50 forced deletes, half of them linked. |
| `delete_parent` | N/5000 forced deletes of a parent with 200 children: each child is unlinked and saved. `us_per_op` is per parent. Only the delete is timed. |

N is 100 000.

## Run

```bash
$YUNETAS_OUTPUTS/bin/perf_tr_treedb             # N = 100000, the reference size
$YUNETAS_OUTPUTS/bin/perf_tr_treedb 20000       # another N
$YUNETAS_OUTPUTS/bin/perf_tr_treedb --small     # N = 10000: what ctest runs
```

Every result is one line of JSON on stdout, e.g.:

```json
{"bench": "perf_tr_treedb", "case": "update_memory", "seconds": 0.268000, "cpu_seconds": 0.267990, "ops": 100000, "us_per_op": 2.680, "cpu_us_per_op": 2.680, "events": 0}
```

`seconds` and `us_per_op` are wall clock time. `cpu_seconds` and
`cpu_us_per_op` are the CPU time of the process (`CLOCK_PROCESS_CPUTIME_ID`).
Compare releases with the CPU time: the wall clock also counts the time that
the other processes of the machine take.

The first line gives `yuneta_version`, `track_memory` and N. The store is
`~/tests_yuneta/perf_tr_treedb`, removed at the end. The benchmark fails
(exit -1) when a write fails, or when memory is not free at the end.

## Against another release

Compile the other release's `tr_treedb.c` (with the headers of
`kernel/c/timeranger2/src`) and put its object before the libraries in the
link line of the benchmark (`build/CMakeFiles/perf_tr_treedb.dir/link.txt`).
Compile the `tr_treedb.c` of this release in the same way, so that the two
binaries differ only in that object.

A difference of a few percent is smaller than the effect of where the linker
puts the code. So link each binary several times, with a small object of a
different size before `tr_treedb.o` (for example `.skip 1649, 0x90` in an
assembler file): 4 layouts are enough. Then:

1. Run the two binaries of a layout one after the other, and swap their
   order at each round. Do a `sync` and a short pause before each run, and
   pin the runs to one CPU (`taskset -c 6`).
2. Do 12 rounds or more of the 4 layouts.
3. Compare `cpu_us_per_op`: the mean +- standard deviation and the median of
   each binary, the change of the median in each layout, and the mean of the
   paired ratio (the two runs of the same round and layout) with its standard
   error.

The figures are in `../README.md`.
