(util-yclone-gclass)=
# `yclone-gclass`

Clone a GClass under a new name, rewriting filenames and the
lower / Capitalized / UPPER identifier variants.

## Usage

```bash
yclone-gclass <OLD_GCLASS> <NEW_GCLASS>
```

```bash
yclone-gclass postgres authenticate     # c_postgres.{c,h} -> c_authenticate.{c,h}
```

Give the names **without** the `c_` prefix or `.c`/`.h` extension; both names
are required. No options beyond argp's built-in `--help` / `--version`.

## See also

- [`yclone-project`](yclone-project.md) — clone a whole project.
- [`utils/c/yclone-gclass/README.md`](https://github.com/artgins/yunetas/blob/7.7.2/utils/c/yclone-gclass/README.md).
