# c_tcp inactivity test

Unit tests for the `C_TCP` `timeout_inactivity` attribute, which makes a pure
tcp client:

1. connect the first time,
2. **close** the connection after `timeout_inactivity` ms of silence,
3. **reconnect on demand** when new tx data arrives, flushing the queued data.

With `timeout_inactivity = -1` (the default) the connection is never closed for
inactivity; these tests use a positive value to exercise the close/reconnect
path.

## Topology

- **Echo server**: `pepon` over `__input_side__`
  (`C_IOGATE` → `C_TCP_S` + `C_CHANNEL` → `C_PROT_RAW` → `C_TCP`). `C_PROT_RAW`
  echoes bytes verbatim, so the raw client gets back exactly what it sent.
- **Client under test**: a pure `C_TCP` created directly as a child of the
  test gclass. It is driven with `EV_TX_DATA` and observed through its output
  events (`EV_CONNECTED` / `EV_DISCONNECTED` / `EV_RX_DATA`). Driving `C_TCP`
  directly (instead of through a protocol stack) is what lets the test inject
  data while the client is disconnected and assert the on-demand reconnection.

## Tests

| Test  | Traffic        | Client url                | What it proves                                         |
|-------|----------------|---------------------------|--------------------------------------------------------|
| test1 | clear (no TLS) | `tcp://127.0.0.1:7779`    | idle-close then reconnect-on-tx, echo round-trip       |
| test2 | secure (TLS)   | `tcps://127.0.0.1:7780`   | same flow across the TLS handshake                     |
| test3 | clear (no TLS) | `tcp://127.0.0.1:7781`    | retry-with-backoff after a FAILED connect              |
| test4 | clear (no TLS) | `tcp://127.0.0.1:7782`    | the pending tx queue survives FAILED reconnects        |

All four use a real `timeout_inactivity` of 20000 ms (OVH idle-drops at ~55 s;
the prod timeout is 30 s). Sub-second values are pointless here: the auto-retry
already runs ~2 s apart and `C_TIMER` has ~1 s resolution.

`test2` exercises the same flow across the TLS handshake: the inactivity timer
is armed in `set_secure_connected` (after the handshake completes) and re-armed
on the reconnect.

`test3` and `test4` are regressions for the inactivity-model reconnection logic
in `set_disconnected()`. They start the client while **no server listens** so
the first connects are refused, then bring the echo server up late:

- **test3** — a failed connect must keep retrying at `timeout_between_connections`
  (it used to clear the timer on any disconnect and stall). Asserts the client
  eventually connects once the server is up.
- **test4** — bytes sent **while disconnected** are queued in `dl_tx` and must
  survive the failed reconnects (the queue used to be flushed on every
  disconnect, silently losing the message). Asserts the queued bytes are flushed
  and echoed once a retry finally connects.

## Flow (event-driven, test1/test2)

1. play → start the echo server, wait, then start the client
2. client connects the first time            → `EV_CONNECTED`
3. no traffic; the client's inactivity timer closes the connection
                                              → `EV_DISCONNECTED`
4. send `EV_TX_DATA` while disconnected: the client reconnects on demand
                                              → `EV_CONNECTED`
5. the queued data is flushed, the server echoes it back
                                              → `EV_RX_DATA`
6. verify the echo and shut down

The assertions are the ordered milestone logs, checked in strict FIFO order by
the test harness. The tests do not assert wall-clock timing: `C_TIMER` rides the
yuno's ~1 s periodic heartbeat, so the flow waits for each event whenever it
arrives.

## Run

The binaries run standalone (exit 0 on success):

```bash
cd build && make
./test_tcp_inactivity_test1
./test_tcp_inactivity_test2
./test_tcp_inactivity_test3
./test_tcp_inactivity_test4
```
