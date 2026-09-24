# c_udp_s_restart test

Tests a stop and a start of `C_UDP_S` (`kernel/c/root-linux/src/c_udp_s.c`),
as logcenter does at `pause-yuno` and `play-yuno`.

A driver gclass (`C_TEST_UDP_RESTART`) creates a `C_UDP_S` child and a plain
UDP socket, the peer:

1. `one` is sent and `C_UDP_S` is stopped at once: the send is in flight and
   the read is canceling. After 100 ms the stop must be done (`ST_STOPPED`,
   and `EV_STOPPED` published once).
2. `/dev/null` is opened -- it takes the number of the socket the stop closed
   -- and `C_UDP_S` is started again; `two` is sent, and the peer sends
   `hello` to the server.
3. A stop and a start in the SAME turn (the read of the stop is still
   canceling), and the peer sends `again`.

At the end the peer must have `one two`, `C_UDP_S` must have received
`hello again`, and be in `ST_IDLE`. Up to 7.25.4:

- the read of the first start was started again on the number of its closed
  socket -- `/dev/null` by then -- and the server stopped reading, with
  nothing logged;
- the datagram being sent at the stop was kept as the one being sent: `two`
  waited behind it for ever;
- a start while the read was canceling re-armed that read (*"is
  CANCELING"*): no read at all;
- the stop published nothing (the event table declared the state
  `ST_STOPPED` as its output event).

## Run

```bash
ctest -R test_c_udp_s_restart --output-on-failure --test-dir build
```
