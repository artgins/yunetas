(util-ymake-skeleton)=
# `ymake-skeleton`

Generate a new skeleton from an existing project, rewriting its role/root
identifiers so the result is a reusable template.

## Usage

```bash
ymake-skeleton <SOURCE-PROJECT> <DESTINATION-PROJECT> --yunorole <role> --rootname <name>
```

| Argument / Option | Purpose |
|-------------------|---------|
| `<SOURCE-PROJECT>` | Existing project directory (positional, required) |
| `<DESTINATION-PROJECT>` | New skeleton directory — must not exist (positional, required) |
| `--yunorole` / `-r` | Role keyword to parameterize (required) |
| `--rootname` / `-n` | Root name keyword(s), comma-separated (required) |

All four are mandatory.

## See also

- [`yuno-skeleton`](yuno-skeleton.md) — instantiate from a skeleton.
- [`utils/c/ymake-skeleton/README.md`](https://github.com/artgins/yunetas/blob/7.5.7/utils/c/ymake-skeleton/README.md).
