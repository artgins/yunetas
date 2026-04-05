# tr2migrate

Migrate data from the legacy **timeranger** (v1) on-disk format to **timeranger2**. Bulk-copies topics, records and indices from a source path to a destination path.

## Usage

```bash
tr2migrate --source <old-tranger> --destination <new-tranger2>
```

Intended as a one-shot upgrade tool.
