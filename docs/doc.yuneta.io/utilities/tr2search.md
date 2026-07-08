(util-tr2search)=
# `tr2search`

Search the records of a timeranger2 topic by content and key, with optional
base64 decoding and rowid/time ranges.

## Usage

```bash
tr2search PATH [options]
```

## Options

| Option | Purpose |
|--------|---------|
| `--search-content-key` | Field/key to search within record content |
| `--search-content-text` | Text to match |
| `--search-content-filter clear\|base64` | Decode content before matching |
| `--diplay-format json\|hexdump` | Output format *(spelled as in the tool)* |
| `--key` / `--not-key` | Key filters |
| `--from-t` / `--to-t`, `--from-tm` / `--to-tm` | Time ranges |
| `--from-rowid` / `--to-rowid` | Rowid range |
| `--user-flag-set` / `--system-flag-set` (+ `…-not-set`) | Flag filters |
| `--fields`, `--mode form\|table`, `--show_md2`, `--recursive`, `--verbose`/`-l` | As in [`tr2list`](#util-tr2list) |

## See also

- [`tr2list`](tr2list.md), [`tr2keys`](tr2keys.md).
- [`utils/c/tr2search/README.md`](https://github.com/artgins/yunetas/blob/7.7.2/utils/c/tr2search/README.md).
