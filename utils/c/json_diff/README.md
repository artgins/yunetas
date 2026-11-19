# json_diff

json_diff compares two JSON files by their content. It can compare arrays as unordered sets. It can also ignore the metadata and private fields, for example when you compare test fixtures or yuno configurations.

## Usage

```bash
json_diff --file1 <a.json> --file2 <b.json> [options]
```

Give the files with `--file1`/`-a` and `--file2`/`-b`. The tool does not read them by position.
Run `json_diff --help` to see all the options (`--without_metadata`/`-m`,
`--without_private`/`-p`, …).
