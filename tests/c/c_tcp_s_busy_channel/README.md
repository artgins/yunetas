# c_tcp_s_busy_channel test

An accept of `C_TCP_S` with `child_tree_filter` never takes a channel whose
`C_TCP` is running.

The filter says nothing of the transport. `{"connected": false}` reads the first
`connected` of the chain, and a protocol can have its own (`C_PROT_MQTT`, removed
after 7.30.26, had one true only at its session).
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
5. A peer connects to `nokw_port` (`127.0.0.1:7880`, in the gate `__nokw__`),
   whose `child_tree_filter` has no `"kw"` (`{"op": "find"}`): every child of
   its gate is a candidate, and its channel is the first one. It is accepted.
   Up to 7.30.14 each such accept leaked one json (the default that
   `kw_get_dict()` handed back): the memory audit at the end fails the test.

`tls_port` needs `/yuneta/agent/certs/localhost.crt` and `.key`. Without them
`C_TCP_S` exits the yuno with code 0 at the start (`exitOnError`, `-1` by
default), which ctest would take for a pass: an `atexit()` handler turns an
exit before the last phase into code 1 (*"the yuno exited before the end of
the test"*). With `"exitOnError": 0` it would not listen and the test would
fail with *"TEST: tls_port is not listening: are
/yuneta/agent/certs/localhost.{crt,key} there?"*.

Run it:

```bash
ctest -R test_c_tcp_s_busy_channel --output-on-failure --test-dir build
```
