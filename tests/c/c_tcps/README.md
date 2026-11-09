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

`test11` (`main_test11.c` + `c_test11.c`, and `test11_mbedtls` when both
backends are built) is the deadline of the TLS handshake,
`timeout_tls_handshake` of `C_TCP`, at 1 s:

| Who | Does | Must |
|---|---|---|
| a wss server of 2 channels (`child_tree_filter`) | gets two raw TCP clients: one sends nothing, one the byte `0x16` | drop both between 0.9 s and 5 s |
| a TLS server of the new method (no `child_tree_filter`) | gets a raw client that sends `hello` | drop it at once (*"TLS handshake rejected"*), and fire NOTHING in the 3 s after: its clisrv stays running between connections, and a deadline left armed would fire there |
| a wss client | connects to the first server, whose channels are free again | open its session, and keep it past the deadline |
| a wss client (`timeout_between_connections` 300) | connects to a plain TCP server that accepts and says nothing | drop the connection between 0.9 s and 5 s, and connect again: the end |

The three deadlines come within 10 s, so they write ONE warning (*"TLS handshake not ended in time, connection dropped"*, `"suppressed": 0`): since the fixes after 7.30.17 the warning is one per 10 s for the whole yuno (`c_tcp`'s `test11` counts the next window).

Up to 7.30.16 `C_TCP` armed nothing while its handshake ran, and
`EV_CONNECTED` (which starts `C_WEBSOCKET`'s `timeout_handshake`) comes only
when it ends: the two raw clients held both channels for ever (red: *"A TLS
handshake that never ends held its connection"*), and the client waited for
ever. Mutants of the fix and what kills them: no timer for a clisrv (that
red), no deadline for a client (*"The wss client did not drop the silent
server and connect again"*), the deadline not cleared when the handshake ends
(*"The wss session was dropped after its handshake"*), not cleared when the
connection ends (*"Cannot connect tcp gobj"* in the quiet wait), no
`EV_TIMEOUT` in `ST_WAIT_HANDSHAKE` (*"Event NOT DEFINED in state"*).

`test12` (`main_test12.c` + `c_test12.c`, plain TCP) is the inactivity timer
of a client after a local drop. The `C_TCP` (`timeout_inactivity` 1000,
`timeout_stop_tx` 0, no bound) connects to a listener of the test with a 4 KB
receive buffer that nobody accepts, and is sent 8 MB at once, so a write
stays in flight. 300 ms later it is dropped: the drop waits in
`ST_WAIT_STOPPED` for that write. 3 s later the `C_TCP` must still be in
`ST_WAIT_STOPPED`, with nothing logged. Up to 7.30.16 the drop left the
inactivity timer armed, and its `EV_TIMEOUT` landed in `ST_WAIT_STOPPED`
(red: *"Event NOT DEFINED in state"*).

`test13` (`main_test13.c` + `c_test13.c`, plain TCP) is `timeout_stop_tx`
on a CLIENT. The `C_TCP` (`timeout_stop_tx` 1000) is sent 8 MB against a
listener of the test with a 4 KB receive buffer that nobody accepts, and is
STOPPED 300 ms later, with a write in flight. The socket's `TCP_USER_TIMEOUT`
must abort that write, with ONE warning (*"The peer did not take the data in
flight in timeout_stop_tx, connection aborted"*), and the stop must end in
`ST_STOPPED` with `EV_STOPPED` within 8 s (the kernel aborts at its next
zero-window probe). Up to 7.30.16 the stop of a client closed its socket
(the connect event's) before looking for it to set `TCP_USER_TIMEOUT`, found
none, said nothing, and waited for ever (red: *"TEST: the stop did not
end"*). A clisrv keeps its socket until its stop ends, which is why
`c_tcp_s_stats` (a clisrv) never saw it.

## Run

```bash
ctest -R '^c_tcps/' --output-on-failure --test-dir build
```

`test9` gives a TLS client a `ssl_trusted_certificate` that does not exist
(`ssl_verify_mode: required`, no server). Its ytls is built when it connects:
one ERROR (*"SSL_CTX_load_verify_locations() FAILED"*), the connection is
refused, and the yuno goes on; after 1000 ms, before the next attempt, the test
ends. Up to 7.30.3 the OpenSSL backend exited the yuno there with code 0 -- the
exit a server that could not load its certificate at its first start took up
to 7.30.17 (see `test14`) -- while the mbedTLS backend only refused the
connection: an agent whose
controlcenter link named a missing file died. An exit with code 0 is a pass for
ctest, so an `atexit()` handler turns an exit before the end of the test into
code 1 (red: *"the yuno exited before the end of the test"*).

`test14` (`main_test14.c` + `c_test14.c`, and `test14_mbedtls` when both
backends are built) is a TLS server without its certificate. Three `C_TCP_S`
share one pool of channels, as the agent's two do: `plain_port` (7882) and two
TLS servers, `tls_port` (7881) and `tls_port2` (7883), whose certificate and
key name files of a dir of the run that are not there yet. The three set
`"exitOnError": 0`, as the agent's do: the listener degrades and the yuno goes
on. With the default (`-1`) a TLS server without its certificate exits the yuno
with code 0, as up to 7.30.17 (`test_exit_on_error`, in
`tests/c/c_tcp_s_addr_in_use`).

| When | Must |
|---|---|
| start | ONE error of the cause and ONE *"TLS server without its TLS context: not listening, trying again"* per TLS server; the yuno goes on |
| 300 ms (a tick) | `plain_port` listens and takes a peer; `tls_port` refuses a connect, `listening` and `tls_ready` false, `use_ssl` true; `reload-certs` answers -1 (the error of the cause again) |
| 4 s later | the try of the timer of `tls_port` came and found nothing changed: it says nothing (the log is checked in order) |
| then | the files are copied; `reload-certs` of `tls_port2` answers 0, it listens at once and takes a peer; `tls_port` waits for its timer |
| next try of `tls_port` | it listens (*"TLS context loaded: listening"*), and a TLS client opens its channel (the handshake ended), with no restart |

Up to 7.30.17 ytls exited the yuno with code 0 at the start, whatever
`exitOnError` said (red: *"the yuno exited before the end of the test"*, on
both backends); without `"exitOnError": 0` C_TCP_S exits there, the same red. Mutants of the fix and
what kills them: listening without a context (a SIGSEGV at the first peer),
no try of the timer (*"tls_port did not listen once its certificate was
there"*), a try that loads unchanged files or a state error at each try (an
error out of order), no `reload-certs` path (*"reload-certs with the files
answers 0"*).

`test10` (`main_test10.c` + `c_test10.c`, and `test10_mbedtls` when both
backends are built) starts a TLS server with `"ssl_verify_mode": "request"` and
two clients: one with the test certificate as its own, and one without a
certificate. The server takes the client's certificate as it comes. It gives
the certificate's `spki_sha256` (the sha256 of the key's SPKI, which
`openssl pkey -pubout -outform DER | sha256sum` also gives) with `verified`
false, and logs NO *"did NOT verify"* warning: the owner pins the key, so a
self-signed certificate is no news. The server also has a CA
(`ssl_trusted_certificate`) that signs the client's certificate: `"request"`
does not use it, so `verified` stays false on both backends (up to 7.30.14
mbedTLS verified the chain against it and said true). The client without a
certificate is let in, with no `peer_cert`. Each check is a TEST FAIL error.

