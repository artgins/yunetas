(util-tr2list)=
# `tr2list`

List the records of a **timeranger2** topic, with rich filtering, field
selection, metadata, and local-time output. The topic/database are deduced from
the `PATH`.

## Usage

```bash
tr2list PATH [options]
```

```bash
tr2list /yuneta/store/.../topic --from-rowid -10            # last 10 records
tr2list /yuneta/store/.../topic --key mykey --mode table
tr2list /yuneta/store/.../topic --follow                    # tail -f
```

## Options

| Option | Purpose |
|--------|---------|
| `--key` / `--not-key` / `--rkey` | Match by key (exact / negated / regex) |
| `--from-t` / `--to-t` | Record-time range |
| `--from-tm` / `--to-tm` | Message-time range |
| `--from-rowid` / `--to-rowid` | Rowid range (negative = from the end) |
| `--user-flag-set` / `--system-flag-set` (+ `…-not-set`) | Flag filters |
| `--filter` | JSON dict of field matches |
| `--fields` | Select output fields |
| `--mode form\|table` | Output layout |
| `--show_md2` | Include `__md2__` metadata |
| `--print-local-time` / `-t` | Local timestamps |
| `--recursive` / `-r` | Walk subtopics |
| `--follow` / `-F` | Live tail (not with `-r`) |
| `--dry-run` / `-n` | Resolve and print the plan only |
| `--list-databases` | List databases under the path |
| `--verbose` / `-l` | Verbosity level |

## See also

- [`tr2keys`](tr2keys.md), [`tr2search`](tr2search.md) — keys-only / content search.
- [timeranger2 + treedb crash course](../../../yunos/c/yuno_agent/YUNO_TREEDB.md).
- [`utils/c/tr2list/README.md`](https://github.com/artgins/yunetas/blob/7.8.3/utils/c/tr2list/README.md).
