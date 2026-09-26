# c_tcps test

Tests for the secure TCP (TLS) client via `C_TCP_S`. Covers handshake, certificate handling and encrypted I/O against a local TLS echo server.

Runs against whichever TLS backend is compiled in (`CONFIG_HAVE_OPENSSL` or `CONFIG_HAVE_MBEDTLS`).

`test5` sends 10 bursts of 400 messages of 64 KB over TLS the moment the
link opens (a client resending its window), checks that every echo comes back
complete, intact and in order, and asserts the client's `C_TCP` never had more
than one write in flight (`max_tx_in_progress == 1`). With two in flight, a
short write's rest went out after a later write and the peer failed the
record MAC ("bad record mac"); the echoes alone rarely show it on a loopback,
the stat always does.

## Run

```bash
ctest -R '^c_tcps/' --output-on-failure --test-dir build
```
