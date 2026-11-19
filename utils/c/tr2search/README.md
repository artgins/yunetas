# tr2search

`tr2search` finds the records of a timeranger2 topic by content filters and by key. It can decode base64 and query a range of rowids. Use it to find records without a scan of the full topic.

## Usage

```bash
tr2search <PATH> [options]
```

`PATH` is a positional argument. The content search uses
`--search-content-key`, `--search-content-text` and
`--search-content-filter clear|base64`. Run `tr2search --help` to see the full
filter syntax.
