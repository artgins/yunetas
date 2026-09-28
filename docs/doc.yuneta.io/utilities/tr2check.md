(util-tr2check)=
# `tr2check`

Check a timeranger2 topic filled by a **load test**, in one pass per key:
record count, duplicates, gaps, out-of-range sequences, checksums, storage
rate and latency percentiles. It answers on the topic itself the questions an
acceptance test usually writes in SQL.

## Usage

```bash
tr2check TOPIC_PATH [options]
```

Every key, the sequence in `seq`, the checksum in `checksum`, 36,000,000
records sent:

```bash
tr2check /yuneta/store/<db>/<owner>/<realm>/tracks/raw_tracks \
    --checksum-field=checksum --expected=36000000 > result.json
echo $?     # 0 pass, 1 a check failed, 2 the topic could not be checked
```

One key, with the figures of each key and at most 5 examples per defect:

```bash
tr2check $TOPIC --key=SIM-0001 --per-key --list-max=5
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

## What each check means

| Check | How | SQL it replaces |
|---|---|---|
| `records` | records read | `SELECT COUNT(*)` |
| `unique`, `duplicated` | sequences sorted: a value seen twice is a duplicate | `GROUP BY id HAVING COUNT(*) > 1` |
| `gaps`, `missing` | a jump in the sorted sequence; with `--expected`, also the lost tail | `N − COUNT(DISTINCT seq)` |
| `out_of_range` | a sequence outside `[seq-from, seq-to]` | `seq < 1 OR seq > N` |
| `corrupted` | sha256 recomputed from the record read | `checksum_stored <> checksum_expected` |
| `rate` | records / (last `__t__` − first `__t__`) | — |
| `latency_ms` | `__t__` (stored) − `__tm__` (message time) | — |
| `unreadable_keys` | a key whose history cannot be read whole (`load_failed`) | — |

- A record that arrives **out of order** is not a duplicate.
- **Checksum**: sha256 (hex) of the record without the checksum field and
  without `__md_tranger__`, dumped as compact JSON with sorted keys
  (`JSON_COMPACT|JSON_SORT_KEYS`). The generator computes it the same way.
- **Latency** is exact to the ms when the topic sets `sf_t_ms` and `sf_tm_ms`,
  and to the second otherwise; `latency_resolution_ms` says which.
  Percentiles are exact up to 10 minutes.
- With `--seq-scope=topic`, all the sequences go in one array: 8 bytes per
  record.

The result is **one JSON document on stdout**; logs go to stderr:

```json
{
    "keys": 4,
    "records": 28,
    "unique": 25,
    "duplicated": 1,
    "gaps": 1,
    "missing": 1,
    "out_of_range": 1,
    "no_seq": 1,
    "corrupted": 1,
    "no_checksum": 0,
    "unreadable_keys": 0,
    "seq_min": 1,
    "seq_max": 10,
    "first_t": "2026-09-21T14:13:21.010Z",
    "last_t": "2026-09-21T14:13:48.040Z",
    "duration_s": 27.03,
    "rate": 1.0358860525342213,
    "latency_resolution_ms": 1,
    "latency_ms": {"mean": 36.42857142857143, "p50": 30, "p90": 80, "p95": 90, "p99": 100, "max": 100, "negative": 0},
    "result": "FAIL",
    "examples": {
        "duplicated": [{"key": "B", "seq": 5}],
        "gaps": [{"key": "B", "from": 8, "to": 8}],
        "out_of_range": [{"key": "D", "rowid": 3, "reason": "no sequence"}, {"key": "D", "seq": 0}],
        "corrupted": [{"key": "B", "rowid": 3, "seq": 3, "reason": "checksum mismatch"}],
        "unreadable_keys": []
    }
}
```

(The topic of `tests/c/timeranger2/test_tr2check.c`, with its planted defects,
run with `--checksum-field=checksum`; `topic`, `seq_field` and `seq_scope` are
left out.)

## See also

- [`tr2list`](tr2list.md) — records one by one. [`tr2keys`](tr2keys.md) — the keys.
