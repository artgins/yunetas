# tr2migrate

`tr2migrate` moves data from the legacy on-disk format of timeranger (v1) to timeranger2. It is an upgrade tool that you run one time.

## Usage

```bash
tr2migrate --source <old-tranger> --destination <new-tranger2>
```

The tool copies the topics, records, and indices from the source path to the destination path.
