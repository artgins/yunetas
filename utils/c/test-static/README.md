# test-static

`test-static` is a minimal "hello world" program. It is a smoke test (a quick test that a build works) for the fully-static build (`CONFIG_FULLY_STATIC=y`).

## Usage

```bash
cd utils/c/test-static/build && make
file static_binary_test     # should report: statically linked
./static_binary_test
```

The source file is `hello.c`, but the built binary is `static_binary_test`.

If the binary is dynamically linked, or if it references `ld-linux-*.so`, the static-build configuration has an error. The top-level `CLAUDE.md` has the full notes on static binaries.

## How it works

If the toolchain and the link flags are correct, the build of this directory makes one statically-linked ELF. That ELF runs on any Linux system of the same architecture. It does not depend on glibc or on the dynamic loader.
