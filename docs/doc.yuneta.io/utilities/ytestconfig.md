(util-ytestconfig)=
# `ytestconfig`

Validate a JSON configuration file by parsing it through Yuneta's [`json_config`](#json_config)
and reporting any syntax/structural error. Exits non-zero on failure — handy in
CI or pre-deploy checks.

## Usage

```bash
ytestconfig <FILE>
```

| Option | Purpose |
|--------|---------|
| `<FILE>` | JSON config to validate (positional, required) |
| `--verbose` / `-l` | Verbose mode |

## See also

- [`json_diff`](json_diff.md) — compare two JSON files.
- [`utils/c/ytestconfig/README.md`](https://github.com/artgins/yunetas/blob/7.5.10/utils/c/ytestconfig/README.md).
