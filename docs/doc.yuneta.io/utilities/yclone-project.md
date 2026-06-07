(util-yclone-project)=
# `yclone-project`

Clone an entire project under a new name, replacing a source keyword with a
destination keyword in both filenames and file contents.

## Usage

```bash
yclone-project <SOURCE-PROJECT> <DESTINATION-PROJECT> --src-keyword <kw> --dst-keyword <kw>
```

| Argument / Option | Purpose |
|-------------------|---------|
| `<SOURCE-PROJECT>` | Existing project directory (positional, required) |
| `<DESTINATION-PROJECT>` | New directory — must not exist (positional, required) |
| `--src-keyword` / `-s` | Keyword to replace (required) |
| `--dst-keyword` / `-d` | Replacement keyword (required) |

Both positionals **and** both keyword options are mandatory.

## See also

- [`yclone-gclass`](yclone-gclass.md) — clone a single GClass.
- [`utils/c/yclone-project/README.md`](https://github.com/artgins/yunetas/blob/7.5.2/utils/c/yclone-project/README.md).
