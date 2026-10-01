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

`test6` stops and starts again, in the same turn, a TLS `C_TCP_S` with
`child_tree_filter` while a connection of it lives, and reloads its
certificates (`reload-certs`). Then a message on the old connection and one
on a new connection must both come back. Freed memory is poisoned
(`tests/c/emailsender/poison_alloc.c`, linked in), so a use of freed memory
reads poison every run. Up to 7.25.20 the start after the stop freed the ytls
(`ytls_cleanup`) the live connection still used, and made a new one: the
next record of that connection was decrypted with freed memory (red: a
SegFault in `ytls_decrypt_data()`).
The echo of `two` must come back on the OLD connection; then that connection
is closed (its tree stopped), and the echo of `three` must come back on the
NEW one. The server's certificates are copies in a dir of the run: the
restart reloads nothing (they did not change: no *"TLS certificates
reloaded"*), `reload-certs` once, and, after the copy is touched, another
stop and start of the server reloads it.

## Run

```bash
ctest -R '^c_tcps/' --output-on-failure --test-dir build
```
