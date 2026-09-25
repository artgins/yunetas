# c_udp_s_rx test

Tests the receive side of `C_UDP_S` (`kernel/c/root-linux/src/c_udp_s.c`).

A driver gclass (`C_TEST_UDP_RX`) creates two children:

1. A `C_GSS_UDP_S` (a `C_UDP_S` and the frames it joins until a NUL, as
   logcenter receives the logs of every yuno). Two peers send the pieces of
   two frames, interleaved: A `a1`, B `b1`, A `a2\0`, B `b2\0`. It must
   publish `a1a2` and `b1b2`, from two channels. `C_GSS_UDP_S` keys its
   channels by the label of the `EV_RX_DATA` gbuffer, the peer; up to 7.25.4
   `C_UDP_S` wrote that label only when tracing, every peer was the channel
   `""`, and the frames came out as `a1b1a2` and `b2`.
2. A `C_UDP_S` with `only_allowed_ips`: the yuno's ip lists, as `C_TCP_S`
   asks them at accept. Peers bound to `127.0.1.5` (not allowed),
   `127.0.1.6` (in the yuno's `allowed_ips`), `127.0.0.1` (the loopback,
   always heard) and `127.0.1.8` (in `allowed_ips` AND in `denied_ips`)
   send one datagram each: it must hear `allowed local`, and drop the first
   (*"UDP_S: Ip not allowed, datagram dropped"*) and the last (*"UDP_S: Ip
   denied, datagram dropped"*: denied wins) with a warning. Up to 7.25.4 the
   attribute was documented and never read, and the deny-list not asked.
3. The refused peers send two more datagrams each: still ONE warning per cause
   (said on the transition, then at most once a minute with the count of the
   drops: the source of a datagram can be forged), and the stat
   `rxRefusedMsgs` counts the six drops. Before this fix every datagram was a
   warning, and no stat counted them.

The loopback is `127.0.0.0/8` on Linux, so the peers need no interface of
their own.

## Run

```bash
ctest -R test_c_udp_s_rx --output-on-failure --test-dir build
```
