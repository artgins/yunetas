# c_udp_s_self_stop test

Tests the stops that `C_UDP_S` (`kernel/c/root-linux/src/c_udp_s.c`) makes by
itself, and its refusal of a secure url.

A driver gclass (`C_TEST_UDP_SELF_STOP`) creates two `C_UDP_S` children and a
plain UDP socket, the peer:

1. The server's `rx_buffer_size` goes down to `0`, and the peer sends `deaf`.
   The host answers IN the gbuffer it got (`EV_TX_DATA` with it), so the next
   read needs a new gbuffer, of 0 bytes: the read cannot be started again
   (*"Cannot start event: gbuffer WITHOUT space to read"*, from
   `yev_start_event()`). `C_UDP_S` must say it (*"UDP: the read cannot be
   started again, the server stops listening"*), stop, and -- the answer still
   in flight -- reach `ST_STOPPED` and publish `EV_STOPPED` when the answer
   completes. The peer must get `deaf`.
2. A `C_UDP_S` with `udps://127.0.0.1:34294` is started: the start must fail
   with *"A secure url (udps://) is not supported by C_UDP_S: there is no
   DTLS"*, and nothing may listen on the port.
3. A `C_UDP_S` whose port (`127.0.0.1:34299`) is held by a socket with no
   `SO_REUSEADDR` is started: ONE ERROR, *"yev_create_accept_event()
   FAILED"* (`errno` 98), `listening` false. Its first try (1 s later)
   fails too and says nothing more (only the `bind()` WARNING); the port is
   released, and the second try listens, with one INFO, *"Listening again
   after a failed listen"*.
4. A `C_UDP_S` with an empty url is started: *"URL NULL"*, the start fails,
   the yuno goes on.
5. A `C_UDP_S` whose read cannot be started (`rx_buffer_size` 0) is started:
   ONE ERROR, *"Cannot start the read of the UDP server"*, and at its first
   try only the cause of
   `yev_start_event()`. With `rx_buffer_size` 4096 its second try listens,
   and *"UDP listening ..."* is said then, once. Up to this fix each try said
   *"UDP listening ..."* and its own ERROR again (red).

The `C_UDP_S` of cases 2 to 5 set `"exitOnError": 0`. With the default
(`TRUE`) they exit the yuno with code 0, as up to 7.30.17 and since 7.30.19
(`test_exit_on_error`, in `tests/c/c_tcp_s_addr_in_use`), which ctest takes
for a pass: an `atexit()` handler turns an exit before the end of the test
into code 1 (red: *"the yuno exited before the end of the test"*). 7.30.18
said cases 3 and 5 as *"Listen failed: not listening, trying again"* and
*"Cannot start the read of the UDP server: not listening, trying again"*.

Up to 7.25.4:

- the failure of the re-arm was ignored: the server stayed in `ST_IDLE`,
  "running", and deaf, with nothing logged;
- a self-stop with a send in flight stayed in `ST_WAIT_STOPPED` for ever: the
  completion of the send took the path of a running server, and `EV_STOPPED`
  was never published;
- `udps://` was accepted: the server listened with no TLS session, and the
  first datagram crashed the yuno (`ytls_decrypt_data()` with a NULL
  session).

## Run

```bash
ctest -R test_c_udp_s_self_stop --output-on-failure --test-dir build
```
