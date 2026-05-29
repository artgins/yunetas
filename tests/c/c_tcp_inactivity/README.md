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

| Test  | Traffic           | Client url                    |
|-------|-------------------|-------------------------------|
| test1 | clear (no TLS)    | `tcp://127.0.0.1:7779`        |
| test2 | secure (TLS)      | `tcps://127.0.0.1:7780`       |

`test2` exercises the same flow across the TLS handshake: the inactivity timer
is armed in `set_secure_connected` (after the handshake completes) and re-armed
on the reconnect.

## Flow (event-driven)

1. play → start the echo server, wait, then start the client
2. client connects the first time            → `EV_CONNECTED`
3. no traffic; the client's inactivity timer closes the connection
                                              → `EV_DISCONNECTED`
4. send `EV_TX_DATA` while disconnected: the client reconnects on demand
                                              → `EV_CONNECTED`
5. the queued data is flushed, the server echoes it back
                                              → `EV_RX_DATA`
6. verify the echo and shut down

The assertions are the ordered milestone logs ("Client connected",
"Inactivity disconnected", "Client reconnected", "Echo received"), checked in
strict FIFO order by the test harness. The test does not assert wall-clock
timing: `C_TIMER` rides the yuno's ~1s periodic heartbeat, so a `timeout_inactivity`
of 1500 ms actually closes at the first tick past 1.5 s — the flow waits for the
event whenever it arrives.

## Run

The binaries run standalone (exit 0 on success):

```bash
cd build && make
./test_tcp_inactivity_test1
./test_tcp_inactivity_test2
```
