# c_tcp_s_stats test

Tests the connection stats of `C_TCP_S` (`kernel/c/root-linux/src/c_tcp_s.c`):
`connxs`, the connections it holds now, and `tconnxs`, the connections it
accepted since it was created.

Two `C_TCP_S`, each in a `C_IOGATE` of its own with 3 channels
(`C_PROT_TCP4H` over `C_TCP`): `legacy_port` on `127.0.0.1:7814` accepts
with `child_tree_filter`; `new_port` on `127.0.0.1:7815` has none, and each
channel's `C_TCP` accepts by itself (the new method). The peers are raw
sockets. The stats are read as attributes (`gobj_read_integer_attr()`) and
as the `stats` answer of the server:

Two more `C_TCP_S`, `shared_a` on `127.0.0.1:7816` and `shared_b` on
`127.0.0.2:7816`, share ONE `C_IOGATE` and its 4 channels: the same port on
two hosts, as the agent's servers share one pool.

1. 2 peers connect to each server: `connxs` 2, `tconnxs` 2;
2. 1 peer of each closes: `connxs` 1, `tconnxs` 2;
3. 1 more peer connects to each: `connxs` 2, `tconnxs` 3;
4. a peer of `shared_a` closes, and `shared_a` is stopped and started again
   in the SAME turn, its accept still being canceled: it listens again when
   that stop ends, a peer connects, `connxs` 2, `tconnxs` 4.

Then a `C_IOGATE` of the new method whose 2 channels have no `C_TCP` (the
`C_TCP_S` creates `clisrv-1` and `clisrv-2`) is stopped with its tree, a third
channel is added, and it is started again: the new clisrv must be
`clisrv-3`, and the clisrvs started again must not leak the accept of their
first start. Last, that `C_TCP_S` is stopped ALONE and started again in the
same turn: its clisrvs stop with it and start again when its stop ends, and a
peer connects (`connxs` 1, `tconnxs` 1).

Then `lone_port` (new method, one channel) is stopped alone, its channel
destroyed while the stop waits for that clisrv, a new channel added, and it
is started again: it must listen. And `names_port` is stopped and DESTROYED:
no clisrv may still name it (`tcp_s`); a `names_port2` made in its place
(often at the same address) takes the clisrvs, and its own stop and start in
the same turn must end, and listen.

Up to 7.25.20 both read 0 always: they were `SDF_STATS` attributes backed by
priv counters that no `mt_reading` served; `connxs` was never decremented
(the `EV_STOPPED` subscription that would have done it was commented out);
and with the new method the server does not see the accepts at all.
A count by local port made `shared_a` and `shared_b` each count the
connections of both; a clisrv of the new method started again overwrote its
first accept event (a leak, and *"Destroying a running event"* at the end).
A stop and a start in the same turn made a second socket whose bind failed
(the yuno exited); a lone stop of a new-method server left its clisrvs
running (*"GObj ALREADY RUNNING"* at the start); a clisrv started again
stayed in `ST_STOPPED` and kept the port bound; and the free of a clisrv's
accept event closed the listening socket's number.

## Run

```bash
ctest -R test_c_tcp_s_stats --output-on-failure --test-dir build
```
