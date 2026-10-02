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

`test8` (`main_test8.c` + `c_test8.c`) is about what a subscriber of a volatile clisrv does from inside its events, with freed memory poisoned. Over TLS its host drops the connection from `EV_CONNECTED` (published from the read that ended the handshake), and destroys the clisrv when it stops: nothing of it may be touched after the publish. In clear its host answers `-1` to each `EV_RX_DATA` (its own error), and the client sends two messages: both must arrive. Up to 7.25.21 a non-zero answer stopped the reading -- neither re-armed nor stopped, the connection hung in silence (red: the second message did not arrive). The TLS path has no red test: in the test the drop was asynchronous (the read was cancelled), and the liveness marker of `set_secure_connected()` covers the synchronous one.

`test7` drops a connection from inside its own `EV_RX_DATA`, over TLS and in
clear. The servers use the legacy method (`child_tree_filter`) with channels
that have no `C_TCP` of their own (`C_CHANNEL` -> `C_WEBSOCKET`), so each
accepted connection gets a VOLATILE `C_TCP`, which `C_WEBSOCKET` destroys when
it publishes `EV_STOPPED`. Each client sends a request that is not HTTP; the
websocket drops the connection while it parses it, so the `C_TCP` is destroyed
inside its own callback. Both clients must be disconnected, and no `C_TCP` must
be left in the channels. Freed memory is poisoned, as in test6. Up to 7.25.21
the `C_TCP` went on using itself after that `EV_STOPPED`: `set_disconnected()`
read its url (red in clear: a SegFault), and over TLS the read path took ytls's
-2222 ("the session was freed inside the callback") for a TLS error, wrote the
cause and stopped the destroyed gobj again (red: a SegFault). `test7_mbedtls`
runs it on mbedTLS when both backends are compiled in.

## Run

```bash
ctest -R '^c_tcps/' --output-on-failure --test-dir build
```
