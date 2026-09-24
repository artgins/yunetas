# perf_rotatory

Benchmark of the rotatory alone (no gclass, no yuno): what one record costs.
The rotatory is the file log of every yuno and the audit of the agent.

| Case | What it measures |
|---|---|
| `audit_record` | A record of 300 bytes written as the agent audit writes it: two `rotatory_write()` calls (the text, then `"\n"`), `LOG_AUDIT`, no header. |
| `audit_record_retention` | The same, with the newfile callback of the audit subscribed (its retention runs at the open). |
| `audit_record_flush` | The same, and `rotatory_flush()` after each record: one `write()` to the file for each record, as the agent audit does since 7.25.5. |
| `log_record` | A record of 300 bytes with its priority header (`LOG_INFO`), one call. |

Each case writes 300 000 records into a new directory, with the mask of the
agent audit (`ZZZ-DD_MM_CCYY.log`).

## Run

```bash
$YUNETAS_OUTPUTS/bin/perf_rotatory            # every case, 300 000 records, 1 round
$YUNETAS_OUTPUTS/bin/perf_rotatory 8          # 8 rounds
$YUNETAS_OUTPUTS/bin/perf_rotatory --small    # 30 000 records: what ctest runs
```

Every result is one line of JSON on stdout, e.g.:

```json
{"bench": "perf_rotatory", "case": "audit_record", "round": 1, "seconds": 0.180195, "records": 300000, "ns_per_record": 600.6}
```

The first line gives `yuneta_version`, `track_memory` and the sizes. The
files are in `~/tests_yuneta/perf_rotatory`, removed at the end.

## Against another release

Compile the other release's `kernel/c/gobj-c/src/rotatory.c` with the headers
of this release, and put its object before the libraries in the link line of
the benchmark (the one in
`build/CMakeFiles/perf_rotatory.dir/link.txt`):

```bash
gcc -O2 -g -DNDEBUG -std=gnu99 -D_GNU_SOURCE -funsigned-char \
    -I$YUNETAS_BASE/outputs_ext/include -I$YUNETAS_BASE/outputs/include \
    -I$YUNETAS_BASE/kernel/c/gobj-c/src -c rotatory_7254.c -o rotatory_7254.o
```

Run the two binaries alternated, 8 rounds or more, with a `sync` and a pause
before each run, and compare mean +- standard deviation (median). The figures
are in `../README.md`.
