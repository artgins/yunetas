# perf_tr_treedb_read

Benchmark of the treedb reads: `tr_treedb` over timeranger2, without the
`C_TREEDB` / `C_NODE` gclasses. It measures what a `list-nodes` or a
`get-node` answers through: the lists of a topic and the views of its nodes
(`node_collapsed_view()`, which collapses the hooks and fkeys of a node into
lists of references). Its companion for the writes is
[`perf_tr_treedb`](../perf_tr_treedb/README.md).

One treedb of two topics: N users, each one linked to a department and one
in four to a second one (`users.departments`, a list fkey ->
`departments.users`), and N/40 departments under a top one
(`departments.department_id` -> `departments.departments`). It is built,
closed, and then:

| Case | What it measures | `ops` |
|---|---|---|
| `reopen` | The open of the treedb: every node and its links. | per node |
| `list_nodes` | `treedb_list_nodes()` of every user. | per user listed |
| `list_filter` | The same with a filter of one column (`{"enabled": false}`). | per user looked at |
| `view_users` | The views of every user, as a list (`node_collapsed_view()`, the default options): each has its departments as references. | per user |
| `view_departments` | The views of every department: each has about 50 users in its hook. | per department |
| `get_view` | `treedb_get_node()` and `node_collapsed_view()` of every user, one by one. | per user |
| `dump_views` | The json text of the views of every user (`json2gbuf()`, compact): what a command answer carries. | per user |

N is 8 000. Each case but `reopen` is repeated 10 times.

## Run

```bash
$YUNETAS_OUTPUTS/bin/perf_tr_treedb_read             # N = 8000, the reference size
$YUNETAS_OUTPUTS/bin/perf_tr_treedb_read 20000       # another N
$YUNETAS_OUTPUTS/bin/perf_tr_treedb_read --small     # N = 2000, 2 repetitions: what ctest runs
```

Every result is one line of JSON on stdout, e.g.:

```json
{"bench": "perf_tr_treedb_read", "case": "view_users", "seconds": 0.799216, "cpu_seconds": 0.794256, "ops": 80000, "us_per_op": 9.990, "cpu_us_per_op": 9.928, "check": 99900}
```

`seconds` and `us_per_op` are wall clock time. `cpu_seconds` and
`cpu_us_per_op` are the CPU time of the process (`CLOCK_PROCESS_CPUTIME_ID`).
Compare releases with the CPU time: the wall clock also counts the time that
the other processes of the machine take. `check` is a count the case found
(nodes listed, references in the views, bytes of the text): it is the same
in every release, so a release that answers something else shows it there.

The first line gives `yuneta_version`, `track_memory` and N. The store is
`~/tests_yuneta/perf_tr_treedb_read`, removed at the end. The benchmark fails
(exit -1) when the treedb cannot be built or opened, or when memory is not
free at the end.

## Against another release

As [`perf_tr_treedb`](../perf_tr_treedb/README.md#against-another-release):
link the benchmark against the `tr_treedb.c` of each release (its object
before the libraries), run the two binaries alternated, and compare the CPU
time. The cases are short, so a machine with noise needs more rounds: 16 on
a VM.
