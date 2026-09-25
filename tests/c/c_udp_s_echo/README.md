# c_udp_s_echo test

Tests an answer written in the gbuffer of an `EV_RX_DATA` of `C_UDP_S`
(`kernel/c/root-linux/src/c_udp_s.c`). The gbuffer carries the peer address,
so a host can answer by sending the kw of the `EV_RX_DATA` back as
`EV_TX_DATA`, as `docs/doc.yuneta.io/api/gclass/transport.md` says.

A driver gclass (`C_TEST_UDP_ECHO`) creates a `C_UDP_S` child and two plain
UDP sockets, peer A and peer B. It answers every datagram in its own gbuffer:

1. Five datagrams (`s1`..`s5`) are sent to peer A and, in the same turn,
   peer A sends `hello` and peer B sends `world`: the answers wait in the
   queue behind `s1`..`s5` while the next datagram is read.
2. Peer B sends `b1`, `b2`, `b3` in one turn: each answer is in flight while
   the next datagram is read.

Peer A must get `s1 s2 s3 s4 s5 hello` and peer B `world b1 b2 b3`, with no
error logged. Up to 7.25.4 `C_UDP_S` cleared the gbuffer after the publish and
read the next datagram into it: an answer was sent empty (dropped, *"Cannot
send datagram: dropped"*) or with the bytes and the peer of another datagram,
and a zero-copy send could read memory the next read was writing. Now, when the
host keeps the gbuffer, the next datagram is read into a new one.

## Run

```bash
ctest -R test_c_udp_s_echo --output-on-failure --test-dir build
```
