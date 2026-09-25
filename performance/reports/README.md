# Performance reports

Every Yuneta release ships a performance report: one self-contained `.html`
page with charts, and the raw figures it was made from, in a `.json` file of
the same name. The page is written for someone who decides whether to adopt
Yuneta. It shows what the release does on one machine, what changed against
the release before, and every loss with its reason.

GitHub shows an `.html` file in the tree as source code. Each report below
has two links: the file, and a rendered view of it (htmlpreview.github.io).

The same figures, release after release, are charted on
[doc.yuneta.io/performance](https://doc.yuneta.io/performance/).

## Reports, newest first

| Release | Against | Report | Rendered | Raw figures |
|---------|---------|--------|----------|-------------|
| 7.25.5 | 7.25.4 | [7.25.5.html](7.25.5.html) | [view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.25.5/performance/reports/7.25.5.html) | [7.25.5.json](7.25.5.json) |

## Make the report of a release

1. Measure the release against the one before: the A/B method of
   [`performance/c/README.md`](../c/README.md) (the module of each release
   linked before the same libraries, the two binaries run alternately). Write
   the figures there and in the CHANGELOG, as for every release.
2. At tag time, run the benchmarks of [`performance/c/`](../c) on the build of
   the new release, in rounds (5 or more), with a `sync` and a pause before
   each run.
3. Write `<version>.json` with the schema below: the A/B figures in `ab`, the
   tag-time figures in `figures`, the binary sizes in `binaries`.
   Keep the `id` of a figure the same from release to release: the trend
   charts join the releases by `id`.
4. Make the page and the charts of the documentation:

   ```bash
   python3 performance/reports/make_report.py <version>   # performance/reports/<version>.html
   python3 performance/reports/make_report.py --docs      # docs/doc.yuneta.io/_static/perf/*.svg
   ```

5. Add the release at the top of the table above, add a row to
   `docs/doc.yuneta.io/performance.md`, attach `<version>.html` to the GitHub
   release, and link it from the release body.

## The json schema (`yuneta-perf-report/1`)

One object per release. Units are always given; a figure never changes its
unit or its `id` between releases.

```json
{
    "schema": "yuneta-perf-report/1",
    "version": "7.25.5",
    "previous": "7.25.4",
    "date": "2026-09-25",
    "machine": {
        "kind": "laptop", "model": "...", "cpu": "...", "cores": 4, "threads": 8,
        "cpu_max_ghz": 4.8, "l3_cache": "12 MiB", "cpu_governor": "...",
        "ram": "31 GiB", "disk": "...", "filesystem": "ext4",
        "kernel": "...", "distro": "...", "glibc": "2.43", "shared": "..."
    },
    "build": {
        "compiler": "gcc 15.2.0", "build_type": "RelWithDebInfo (-O2 -g -DNDEBUG)",
        "fully_static": true, "debug_track_memory": true,
        "debug_with_backtrace": true, "tls": "...", "note": "..."
    },
    "method": { "ab": "...", "tag_run": "..." },
    "ab": [
        {
            "id": "tr2.open_master",
            "group": "timeranger2",
            "bench": "perf_timeranger2",
            "case": "open_master",
            "label": "Open a store as master (20 000 md2 files)",
            "meaning": "The start-up time of a yuno that owns a large store.",
            "unit": "ms",
            "better": "lower",
            "previous": { "mean": 92.5, "sd": 1.9, "median": 91.7 },
            "current":  { "mean": 80.8, "sd": 1.9, "median": 80.1 },
            "n": 10,
            "scope": "release",
            "method": "...",
            "verdict": "gain",
            "reason": "...",
            "change_pct": -12.6
        }
    ],
    "figures": [
        {
            "id": "treedb.update_saved",
            "group": "treedb",
            "bench": "perf_tr_treedb",
            "case": "update_saved",
            "label": "Update a node and save it",
            "meaning": "The common write: change a node and persist it.",
            "unit": "us",
            "better": "lower",
            "source": "tag-run",
            "mean": 10.3692, "median": 10.361, "min": 10.228, "max": 10.478, "sd": 0.1053, "n": 5,
            "headline": true
        }
    ],
    "binaries": [
        {
            "name": "mqtt_broker", "kind": "yuno", "bytes": 34657192,
            "stripped_bytes": 10402472, "text": 9549659, "data": 837028, "bss": 114252,
            "static": true, "tls": "openssl"
        }
    ]
}
```

| Field | Meaning |
|-------|---------|
| `schema` | `yuneta-perf-report/<n>`. A change that breaks a reader raises `<n>`. |
| `version`, `previous` | The release measured, and the release its A/B figures compare against. |
| `machine`, `build` | Where and how it was measured. Absolute figures compare only between reports of the same machine. |
| `method.ab`, `method.tag_run` | How the two kinds of figures were measured, in words. |
| `ab[]` | One A/B figure: the same benchmark on `previous` and on `version`. |
| `ab[].id`, `figures[].id` | Stable name, `<area>.<case>` (`tr2.open_master`). The trend charts join releases by it; an `ab` and a `figures` entry of the same measure share it. |
| `ab[].label`, `ab[].short`, `ab[].meaning` | The name of the figure, an optional short name for narrow charts, and what it means for a user. |
| `ab[].unit` | `ms`, `us`, `ns`, `s`, `appends/s`, `K msg/s`, `round trips/s`, ... |
| `ab[].better` | `lower` (a time, a cost) or `higher` (a rate). |
| `ab[].previous`, `ab[].current` | `mean`, and `sd` (standard deviation) and `median` when measured. |
| `ab[].n` | Rounds (or runs) behind each mean. |
| `ab[].scope` | `release`: the module of each release. `release (separate runs)`: release against release, not one alternated run. `change`: one commit alone, before and after. `ctest trend`: times of `yunetas test` runs. |
| `ab[].verdict` | `gain`, `noise` (inside the spread), `price` (slower, kept by decision), `price-until-migrated`. |
| `ab[].reason` | Why it moved, as the CHANGELOG states it. Every `price` has one. |
| `ab[].commit` | For `scope: change`, the commit measured. |
| `ab[].change_pct` | `(current - previous) / previous x 100` of the means, in the figure's own unit. |
| `figures[]` | One absolute figure of `version`, measured at tag time: `mean`, `sd`, `median`, `min`, `max`, `n`. `derived` says how a rate was computed from a benchmark's output. `headline` marks the figures shown first. |
| `binaries[]` | Size of each static executable: `bytes` on disk (with debug information), `stripped_bytes`, and the `text` / `data` / `bss` of `size`. |
