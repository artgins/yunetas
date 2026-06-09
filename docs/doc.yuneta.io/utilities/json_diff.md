(util-json_diff)=
# `json_diff`

Semantic diff of two JSON files (not a persistence reader). Optionally ignores
metadata (`__*`) and private (`_*`) fields before comparing.

## Usage

```bash
json_diff --file1 <a.json> --file2 <b.json> [options]
```

Files are passed via flags — `json_diff` takes **no positional arguments**.

| Option | Purpose |
|--------|---------|
| `--file1` / `-a` | First JSON file |
| `--file2` / `-b` | Second JSON file |
| `--without_metadata` / `-m` | Drop `__*` fields before diffing |
| `--without_private` / `-p` | Drop `_*` fields before diffing |
| `--verbose` / `-v` | Verbose output |

## See also

- [`utils/c/json_diff/README.md`](https://github.com/artgins/yunetas/blob/7.5.12/utils/c/json_diff/README.md).
