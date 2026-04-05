# test-static

Minimal "hello world" program used as a smoke test for the **fully-static build** (`CONFIG_FULLY_STATIC=y`). If the toolchain and link flags are set up correctly, building this directory produces a single statically-linked ELF that runs on any Linux system of the same architecture without depending on glibc or the dynamic loader.

## Usage

```bash
cd utils/c/test-static/build && make
file hello           # should report: statically linked
./hello
```

If the resulting binary is dynamically linked or references `ld-linux-*.so`, something is wrong with the static-build configuration — see the top-level `CLAUDE.md` for the full static-binary notes.
