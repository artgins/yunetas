# yscapec

Escape a file's contents into a C string literal, with configurable line width. Handy when embedding text (JSON config, SQL, HTML, …) directly inside `.c` source.

## Usage

```bash
yscapec <file> [--width <n>]
```

Output goes to stdout; redirect into a header or source file as needed.
