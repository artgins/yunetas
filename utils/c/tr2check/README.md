# tr2check

Check a **timeranger2** topic filled by a load test, in one pass per key:
record count, duplicates, gaps, out-of-range sequences, checksums, storage
rate and latency percentiles. It answers the questions an acceptance test
usually asks in SQL (`COUNT(*)`, `GROUP BY id HAVING COUNT(*) > 1`, …) on the
topic itself.

## Usage

```bash
tr2check <TOPIC_PATH> [options]
```

```bash
# Every key, sequence in the "seq" field, checksum in "checksum",
# 36,000,000 records sent
tr2check /yuneta/store/<db>/<owner>/<realm>/tracks/raw_tracks \
    --checksum-field=checksum --expected=36000000 > result.json
echo $?     # 0 pass, 1 a check failed, 2 the topic could not be checked
```

| Option | Default | Purpose |
|---|---|---|
| `--key=KEY` / `-k` | all | Check only this key |
| `--rkey=REGEX` | all | Check only the keys matching it (POSIX extended) |
| `--seq-field=FIELD` | `seq` | Field with the sequence number |
| `--seq-scope=key\|topic` | `key` | One sequence per key, or one for the whole topic |
| `--seq-from=N` | `1` | First valid sequence |
| `--seq-to=N` | highest seen | Last valid sequence; the missing tail counts as a gap |
| `--expected=N` | — | Distinct sequences sent; `missing` = N − unique |
| `--checksum-field=FIELD` | not checked | Field with the sha256 of the record |
| `--from-t` / `--to-t` | — | Storage time window |
| `--list-max=N` | `20` | Examples listed per defect |
| `--per-key` | off | Add the figures of every key |

- **Sequences** are collected, sorted and walked: a value seen twice is a
  duplicate, a jump is a gap, a value outside `[seq-from, seq-to]` is out of
  range. A record that arrives out of order is not a duplicate.
- **Checksum**: sha256 (hex) of the record without the checksum field and
  without `__md_tranger__`, dumped as compact JSON with sorted keys
  (`JSON_COMPACT|JSON_SORT_KEYS`). The generator must compute it the same way.
- **Latency**: `__t__` − `__tm__` in ms. Exact to the ms when the topic sets
  `sf_t_ms` and `sf_tm_ms`, to the second otherwise
  (`latency_resolution_ms`). Percentiles are exact up to 10 minutes.
- **Rate**: records / (last `__t__` − first `__t__`).

The result is one JSON document on stdout (logs go to stderr):

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

With `--seq-scope=topic` all the sequences go in one array: 8 bytes per record.
