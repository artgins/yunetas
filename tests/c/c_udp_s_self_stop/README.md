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
