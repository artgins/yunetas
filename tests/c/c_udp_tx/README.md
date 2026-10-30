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
3. stops the `C_UDP`: it must end in `ST_STOPPED` and publish `EV_STOPPED`
   once. The yuno dies; the memory check at the end catches a leaked event or
   gbuffer.

Against SDK 7.29.3 the test does not build (`c_udp.h` declared
`C_PROT_UDP`, not `C_UDP`), and with that fixed `register_c_udp()` fails
(*"SMachine: input_list's event NOT in state"*, `EV_STOPPED`). Behind those:
the completion of the connect was *"UDP: event type NOT IMPLEMENTED"*, only
`one` was sent and no `EV_TX_READY` came (the completion of a send asked for
`ST_CONNECTED`, a state `C_UDP` does not have), the stop published no
`EV_STOPPED`, the read event leaked, and `txBytes` read 0.

## Run

```bash
ctest -R test_c_udp_tx --output-on-failure --test-dir build
```
