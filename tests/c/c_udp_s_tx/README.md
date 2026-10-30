# c_udp_s_tx test

Tests the transmit path of `C_UDP_S` (`kernel/c/root-linux/src/c_udp_s.c`).

A driver gclass (`C_TEST_UDP_TX`) creates a `C_UDP_S` child and sends it these
datagrams with `EV_TX_DATA`:

1. a gbuffer with NO peer address: `yev_start_event()` refuses the sendmsg
   (*"Cannot start event: sendmsg addr NULL or bad addr length"*), and
   `C_UDP_S` drops the datagram with an ERROR (*"Cannot send datagram:
   dropped"*);
2. a datagram to the port 0 of the peer: the KERNEL refuses the sendmsg
   (`EINVAL`), and `C_UDP_S` drops it with a WARNING that names the peer and
   the cause (*"UDP: datagram refused by the kernel, dropped"*);
3. an EMPTY gbuffer, to that socket: nothing to send, ignored with a WARNING
   (*"udp tx data empty, ignored"*). Up to 7.29.3 it was a send that cannot
   start: two errors;
4. `one`, to a plain UDP socket of the test;
5. `two`, to the same socket, and the same gbuffer once more: the second send
   is refused (-1, *"udp tx data already queued or in flight, refused"*). Up
   to 7.29.3 it was queued again, which failed and lost a reference.

After 500 ms the socket must hold `one two`, in order, and `C_UDP_S` must
still be listening (`ST_IDLE`). Then, in ONE turn, `C_UDP_S` is sent `gone`,
stopped and DESTROYED, before its `EV_STOPPED`, with that send in flight: no
error, and the destroy cancels the send, which the loop frees with no callback.
Up to 7.29.3 the send events were not kept, and the completion ran on the
freed gobj (red: a SegFault); and the destroy was an ERROR (*"GObj NOT
STOPPED. UV handler ACTIVE!"*, left from the libuv port; red: an unexpected
error). The yuno
dies 300 ms later; the memory check at the end catches a leaked send event or
gbuffer.

Up to 7.25.4 all three failed:

- a send that did not start left its event and its gbuffer allocated,
  `tx_in_progress` never went back to 0 (a stop waited for ever), and
  `gbuf_txing` held every later datagram in the queue;
- the send completion asked for `ST_CONNECTED` before sending the next
  datagram, a state `C_UDP_S` does not have: the first datagram was sent, and
  every later one waited in the queue for ever;
- a send the kernel refused was taken as a disconnection: the whole server
  stopped, reading too, and only a trace said so.

## Run

```bash
ctest -R test_c_udp_s_tx --output-on-failure --test-dir build
```
