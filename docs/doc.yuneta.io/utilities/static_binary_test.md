(util-static_binary_test)=
# `static_binary_test`

A minimal "hello world" smoke test that confirms the fully-static build
(`CONFIG_FULLY_STATIC`) produces a self-contained ELF. (Source dir:
`utils/c/test-static`, file [`hello.c`](https://github.com/artgins/yunetas/blob/7.5.5/utils/c/test-static/hello.c); the built binary is
`static_binary_test`.)

## Usage

```bash
static_binary_test            # prints a greeting
file static_binary_test       # should report "statically linked"
```

No arguments. Use it to verify a static toolchain/link is healthy.

## See also

- [Fully Static Binaries](../installation.md) — the `CONFIG_FULLY_STATIC` build.
- [`utils/c/test-static/README.md`](https://github.com/artgins/yunetas/blob/7.5.5/utils/c/test-static/README.md).
