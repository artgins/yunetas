(util-yscapec)=
# `yscapec`

Escape a file's contents into a **C string literal** printed to stdout — handy
for embedding JSON, SQL or HTML in `.c` source (e.g. a yuno's `fixed_config`).

## Usage

```bash
yscapec <FILE> > embedded.h
```

| Option | Purpose |
|--------|---------|
| `<FILE>` | Input file (positional, required) |
| `--no-conversion` / `-n` | Keep `"` as `\"` instead of converting to `'` |
| `--line-size` / `-s` | Output line width (default `70`) |

Tabs become 4 spaces and control characters become `\ooo` octal escapes.

## See also

- [`utils/c/yscapec/README.md`](https://github.com/artgins/yunetas/blob/7.6.8/utils/c/yscapec/README.md).
