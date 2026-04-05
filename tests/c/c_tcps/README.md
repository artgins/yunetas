# c_tcps test

Tests for the secure TCP (TLS) client via `C_TCP_S`. Covers handshake, certificate handling and encrypted I/O against a local TLS echo server.

Runs against whichever TLS backend is compiled in (`CONFIG_HAVE_OPENSSL` or `CONFIG_HAVE_MBEDTLS`).

## Run

```bash
ctest -R test_c_tcps --output-on-failure --test-dir build
```
