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

```bash
ymake-skeleton ./gate_frigo ./skeletons/yuno_gate --yunorole=gate_frigo --rootname=gate_frigo,frigo
```

It is the reverse of [`yuno-skeleton`](yuno-skeleton.md): each keyword
becomes a template marker -- `{{yunorole}}` / `{{Rootname}}` / `{{ROOTNAME}}`
inside text files, `+yunorole+` / `+rootname+` in file and directory names
-- and every text file takes the `_tmpl` suffix. A keyword is replaced only as
a whole word, the same rules as [`yclone-project`](yclone-project.md) (binary
files copied as they are, `.git`/`build`/`node_modules`… not cloned). When
`yunorole` and `rootname` are the same word, `yunorole` takes it.

The new directory is a template, not yet a skeleton the tool lists: add an
entry for it in the `__skeletons__.json` of its directory (copy the one of
`yuno_standalone` and change its `name`), then `yuno-skeleton -p <dir>
<name>`.

A `{{` that the source already holds (an i18next `{{name}}` in a JS file) is
read by `yuno-skeleton` as a marker, and comes out empty: check for it
before you publish the template.

## See also

- [`yuno-skeleton`](yuno-skeleton.md) — instantiate from a skeleton.
- [`utils/c/ymake-skeleton/README.md`](https://github.com/artgins/yunetas/blob/7.28.8/utils/c/ymake-skeleton/README.md).
