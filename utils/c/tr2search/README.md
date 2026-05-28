# tr2search

Search records inside a **timeranger2** topic by content filters and key, with optional base64 decoding and rowid range queries. Useful for locating records without scanning the whole topic.

## Usage

```bash
tr2search <PATH> [options]
```

`PATH` is a positional argument. Content search uses
`--search-content-key`, `--search-content-text` and
`--search-content-filter clear|base64`. Run `tr2search --help` for the full
filter syntax.
