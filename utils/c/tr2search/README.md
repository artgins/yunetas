# tr2search

Search records inside a **timeranger2** topic by content filters and key, with optional base64 decoding and rowid range queries. Useful for locating records without scanning the whole topic.

## Usage

```bash
tr2search --path <tranger-path> --topic <topic> [--filter <expr>] [options]
```

Run `tr2search --help` for the full filter syntax.
