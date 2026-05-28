# json_diff

Semantic diff of two JSON files. Arrays can be compared as unordered sets, and metadata / private fields can be filtered out — convenient when comparing test fixtures or yuno configurations.

## Usage

```bash
json_diff --file1 <a.json> --file2 <b.json> [options]
```

The files are passed via `--file1`/`-a` and `--file2`/`-b` (not positionally).
Run `json_diff --help` for the full list of options (`--without_metadata`/`-m`,
`--without_private`/`-p`, …).
