# yscapec

`yscapec` writes the contents of a file as a C string literal. You can set the line width. Use it to put text (JSON configuration, SQL, HTML, or other text) directly in a `.c` source file.

## Usage

```bash
yscapec <file> [--width <n>]
```

The tool writes to stdout. To keep the output, redirect it into a header or a source file.
