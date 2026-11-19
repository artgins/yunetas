# tr2keys

`tr2keys` lists all keys (primary ids) of a timeranger2 topic. It is like `tr2list`, but it prints only the set of keys. Use it to audit a topic or to give the keys to a batch job.

## Usage

```bash
tr2keys <PATH> [options]
```

`PATH` is a positional argument. Active options: `--recursive`/`-r`,
`--list-databases`. Run `tr2keys --help` for the full list.
