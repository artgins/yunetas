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
| `--from-t` / `--to-t`, `--from-tm` / `--to-tm` | Time ranges, read as [`tranger2_time_of_text()`](<#tranger2_time_of_text>) reads a time bound and passed to the tranger as text: the text of a JSON integer in the topic's unit (`1790812800`, or ms on an `sf_t_ms` topic; `-86400` counts back from the last record of each key), or a date (`2026-10-01`, `-1d`, `"2 hours ago"`), taken to the unit of its axis. A number of another shape (`007`, `" 5"`, `10.5`, `1,5`) or a text that is no date is refused, saying why: `tr2search ... --from-t=1,5` answers *"TIME of --from-t refused, a number that is not the text of a JSON integer: '1,5'"*. Up to 7.29.3 `007` was 7, `" 5"` and `1,5` dates, `-86400` refused, and a date stayed in seconds on a topic in milliseconds. With `--recursive` each topic reads the bound in its own unit: `tr2search <database> -r --from-t=2026-10-01 ...` lists the same records of an `sf_t_ms` topic as `tr2search <database>/<topic> --from-t=2026-10-01 ...`; up to 7.29.3 every topic read it in the unit of the first one listed, and a topic in seconds after one in milliseconds listed nothing |
| `--from-rowid` / `--to-rowid` | Rowid range |
| `--user-flag-set` / `--system-flag-set` (+ `…-not-set`) | Flag filters |
| `--fields`, `--mode form\|table`, `--show_md2`, `--recursive`, `--verbose`/`-l` | As in [`tr2list`](#util-tr2list) |

## See also

- [`tr2list`](tr2list.md), [`tr2keys`](tr2keys.md).
- [`utils/c/tr2search/README.md`](https://github.com/artgins/yunetas/blob/7.29.3/utils/c/tr2search/README.md).
