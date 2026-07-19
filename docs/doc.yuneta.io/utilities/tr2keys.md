(util-tr2keys)=
# `tr2keys`

List the **key set** (primary ids) of a timeranger2 topic — the same backing
store as [`tr2list`](tr2list.md), but only the keys.

## Usage

```bash
tr2keys PATH [options]
```

| Option | Purpose |
|--------|---------|
| `--recursive` / `-r` | Walk subtopics |
| `--list-databases` | List databases under the path |

The topic/database are deduced from `PATH`.

## See also

- [`tr2list`](tr2list.md) — full records; [`tr2search`](tr2search.md) — content search.
- [`utils/c/tr2keys/README.md`](https://github.com/artgins/yunetas/blob/7.8.2/utils/c/tr2keys/README.md).
