# c_tcp_s_busy_channel test

An accept of `C_TCP_S` with `child_tree_filter` never takes a channel whose
`C_TCP` is running.

The filter says nothing of the transport. `{"connected": false}` reads the first
`connected` of the chain, and `C_PROT_MQTT` has its own, true only at its session.
This test gets the same with no MQTT: its filter is `{"opened": false}`, and
`C_CHANNEL` is opened only when the TLS handshake ends.

Two `C_TCP_S` share ONE channel (`C_PROT_TCP4H` over `C_TCP`): `tls_port` on
`127.0.0.1:7878` and `plain_port` on `127.0.0.1:7879`. The peers are raw sockets.

1. A peer connects to `tls_port` and never starts the handshake.
2. A peer connects to `plain_port`: refused, `noChannelConnxs` is 1. Up to 7.30.1
   it took the running channel (*"GObj ALREADY RUNNING"*) and wrote the NULL `ytls`
   of `plain_port` over the one of the first peer.
3. The first peer leaves. Up to 7.30.1 this was a SIGSEGV in
   `ytls_free_secure_filter()`.
4. A third peer connects to `plain_port`: accepted, the channel is free again.

Run it:

```bash
ctest -R test_c_tcp_s_busy_channel --output-on-failure --test-dir build
```
