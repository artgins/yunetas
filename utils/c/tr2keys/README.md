# tr2keys

List all keys (primary ids) stored in a **timeranger2** topic. Supports the same time-range and rowid filters as `tr2list`, but outputs only the key set — useful for auditing or batch processing.

## Usage

```bash
tr2keys --path <tranger-path> --topic <topic> [options]
```

Run `tr2keys --help` for all options.
