# tr2keys

List all keys (primary ids) stored in a **timeranger2** topic — like `tr2list`, but outputs only the key set. Useful for auditing or batch processing.

## Usage

```bash
tr2keys <PATH> [options]
```

`PATH` is a positional argument. Active options: `--recursive`/`-r`,
`--list-databases`. Run `tr2keys --help` for the full list.
