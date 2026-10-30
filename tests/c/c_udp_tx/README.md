# c_udp_tx test

Tests the transmit path of `C_UDP` (`kernel/c/root-linux/src/c_udp.c`), the
UDP client. `C_UDP` is not registered by the SDK: the test includes
`c_udp.h` and calls `register_c_udp()`.

A driver gclass (`C_TEST_UDP_CLIENT`) creates a `C_UDP` child whose url is a
plain UDP socket of the test, bound to a port of the kernel's choice, and:

1. sends an EMPTY gbuffer, with nothing in flight: it is ignored with a
   WARNING (*"udp tx data empty, ignored"*), and its `EV_TX_READY` must come
   one turn later, never inside the send;
2. sends `one`, `two` and `three` in one turn: after 300 ms the socket must
   hold `one two three`, in order, `EV_TX_READY` must have come once, and
   the stats must say 3 datagrams (`txMsgs`) and 11 bytes (`txBytes`);
3. sends `four`, then in the same turn stops the `C_UDP` (it waits in
   `ST_WAIT_STOPPED` for the send and the cancel of the read), sends `late`
   (dropped: ONE WARNING when the stop ends, *"udp data sent while the client
   stops, dropped"*) and starts it again (`gobj_start()` must answer 0). A
   second `C_UDP` is sent `gone`, stopped and DESTROYED in that turn, with its
   send in flight (an ERROR, *"GObj NOT STOPPED. UV handler ACTIVE!"*). After
   300 ms the first must be running in `ST_IDLE`, with no `EV_STOPPED` (the
   start came before the stop ended), and the socket must hold `four`: the
   socket closes when the stop ends;
4. sends `five`: the socket must hold it;
5. stops the `C_UDP`: it must end in `ST_STOPPED` and publish `EV_STOPPED`
   once. The yuno dies; the memory check at the end catches a leaked event or
   gbuffer.

In the fixes after 7.29.3 (red): `late` was *"Event NOT DEFINED in state"*,
the start was refused (*"Initial wrong udp state"*) and left the gobj running,
and the completion of the send of the destroyed `C_UDP` ran on the freed gobj
(a SegFault).

A second binary, `test_c_udp_tx_refused` (`C_TEST_UDP_REFUSED`), points a
`C_UDP` at a port of the loopback where nobody listens (one the kernel gave to
a socket of the test, closed before the start) and sends it 50 datagrams in one
turn. The kernel answers the first with "port unreachable", and the read fails
with `ECONNREFUSED` while the next datagram is in flight: the `C_UDP` stops by
itself (*"UDP: read FAILED, the client stops"*, the only ERROR it may log; the
WARNINGs of datagrams the kernel refuses are the kernel's to count). 500 ms
later it must be in `ST_STOPPED`, NOT running, with ONE `EV_STOPPED`, and a
`gobj_start()` must work; a stop then publishes a second `EV_STOPPED`. In the
fixes after 7.29.3 (red) it stayed in `ST_WAIT_STOPPED` for ever, and the start
said *"GObj ALREADY RUNNING"*.

Against SDK 7.29.3 the test does not build (`c_udp.h` declared
`C_PROT_UDP`, not `C_UDP`), and with that fixed `register_c_udp()` fails
(*"SMachine: input_list's event NOT in state"*, `EV_STOPPED`). Behind those:
the completion of the connect was *"UDP: event type NOT IMPLEMENTED"*, only
`one` was sent and no `EV_TX_READY` came (the completion of a send asked for
`ST_CONNECTED`, a state `C_UDP` does not have), the stop published no
`EV_STOPPED`, the read event leaked, and `txBytes` read 0.

## Run

```bash
ctest -R 'test_c_udp_tx' --output-on-failure --test-dir build    # both binaries
```
