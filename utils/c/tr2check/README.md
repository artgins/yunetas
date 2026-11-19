# tr2check

`tr2check` examines a timeranger2 topic that a load test filled, in one pass
for each key. It counts the records, the duplicates, the gaps, the sequences
out of range, the checksums, the storage rate and the latency percentiles.
An acceptance test usually asks these questions in SQL (`COUNT(*)`,
`GROUP BY id HAVING COUNT(*) > 1`, …). `tr2check` answers them on the topic
itself.

## Usage

```bash
tr2check <TOPIC_PATH> [options]
```

```bash
# Every key, sequence in the "seq" field, checksum in "checksum",
# 36,000,000 records sent
tr2check /yuneta/store/<db>/<owner>/<realm>/events/raw_events \
    --checksum-field=checksum --expected=36000000 > result.json
echo $?     # 0 pass, 1 a check failed, 2 the topic could not be checked
```

`TOPIC_PATH` is `<directory>/<database>/<topic>`. It can be absolute or
relative to the current directory. For example, `tr2check raw_events` works
from inside the database. The tool resolves the path first. Then it takes the
database and the directory above it from the resolved path. Up to 7.25.20 it
took them from the text. Thus a relative `db/topic` or a bare `topic` opened a
directory that does not exist.

The exit codes are:

- 0: every check passes.
- 1: a check failed. A topic with no record fails, because the tool examined nothing.
- 2: the tool cannot examine the topic. The path is not a topic, or the path
  does not resolve, or the memory cannot hold all the sequences of a key.
  In the last case, the load of the key stops there. A result on part of the
  key is not true, so the tool gives none. Up to 7.25.20 it said PASS in that case.

The tool never writes to the store. It raises its soft limit of open files
to the hard limit, because timeranger2 keeps the files of every key open. If
it cannot, it writes a message on stderr. It never raises the HARD limit, also
when it runs as root. If a topic has more keys than the hard limit, raise that
limit before the run (`ulimit -Hn`, or `/etc/security/limits.d/`).

Up to 7.25.20 the tool set both limits to 200000. For root, that raised them.
For any other user with a lower hard limit, it failed silently. Where the hard
limit was higher, it made the hard limit lower.

| Option | Default | Purpose |
|---|---|---|
| `--key=KEY` / `-k` | all | Check only this key |
| `--rkey=REGEX` | all | Check only the keys matching it (POSIX extended) |
| `--seq-field=FIELD` | `seq` | Field with the sequence number |
| `--seq-scope=key\|topic` | `key` | One sequence per key, or one for the whole topic |
| `--seq-from=N` | `1` | First valid sequence |
| `--seq-to=N` | highest seen | Last valid sequence. The missing tail counts as a gap |
| `--expected=N` | none | Distinct sequences sent. `missing` = N − unique |
| `--checksum-field=FIELD` | not checked | Field with the sha256 of the record |
| `--from-t` / `--to-t` | none | Storage time window |
| `--list-max=N` | `20` | Examples listed per defect |
| `--per-key` | off | Add the figures of every key |

The figures are:

- Sequences: the tool collects the sequences, sorts them and reads them in
  order. A value seen two times is a duplicate. A jump is a gap. A value
  outside `[seq-from, seq-to]` is out of range. A record that arrives out of
  order is not a duplicate.
- Checksum: the sha256 (hex) of the record without the checksum field and
  without `__md_tranger__`, dumped as compact JSON with sorted keys
  (`JSON_COMPACT|JSON_SORT_KEYS`). The generator must calculate it the same way.
- Latency: `__t__` − `__tm__` in ms. If the topic sets `sf_t_ms` and
  `sf_tm_ms`, the latency is exact to the ms. If not, it is exact to the
  second (`latency_resolution_ms`). Percentiles are exact up to 10 minutes.
- Rate: records / (last `__t__` − first `__t__`).

The result is one JSON document on stdout. The logs go to stderr:

```json
{
    "keys": 3000, "records": 36000000, "unique": 36000000, "expected": 36000000,
    "duplicated": 0, "gaps": 0, "missing": 0, "out_of_range": 0, "no_seq": 0,
    "corrupted": 0, "no_checksum": 0, "unreadable_keys": 0,
    "seq_min": 1, "seq_max": 12000,
    "first_t": "2026-10-05T10:00:00.004Z", "last_t": "2026-10-05T10:30:00.001Z",
    "duration_s": 1799.997, "rate": 20000.03,
    "latency_resolution_ms": 1,
    "latency_ms": {"mean": 12.4, "p50": 9, "p90": 21, "p95": 28, "p99": 57, "max": 410, "negative": 0},
    "result": "PASS",
    "examples": {"duplicated": [], "gaps": [], "out_of_range": [], "corrupted": [], "unreadable_keys": []}
}
```

With `--seq-scope=topic`, all the sequences go in one array. The array takes 8 bytes for each record.
