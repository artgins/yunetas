# c_tcp_s_stats test

Tests the connection stats of `C_TCP_S` (`kernel/c/root-linux/src/c_tcp_s.c`):
`connxs`, the connections it holds now, and `tconnxs`, the connections it
accepted since it started.

Two `C_TCP_S`, each in a `C_IOGATE` of its own with 3 channels
(`C_PROT_TCP4H` over `C_TCP`): `legacy_port` on `127.0.0.1:7814` accepts
with `child_tree_filter`; `new_port` on `127.0.0.1:7815` has none, and each
channel's `C_TCP` accepts by itself (the new method). The peers are raw
sockets. The stats are read as attributes (`gobj_read_integer_attr()`) and
as the `stats` answer of the server:

1. 2 peers connect to each server: `connxs` 2, `tconnxs` 2;
2. 1 peer of each closes: `connxs` 1, `tconnxs` 2;
3. 1 more peer connects to each: `connxs` 2, `tconnxs` 3.

Up to 7.25.20 both read 0 always: they were `SDF_STATS` attributes backed by
priv counters that no `mt_reading` served; `connxs` was never decremented
(the `EV_STOPPED` subscription that would have done it was commented out);
and with the new method the server does not see the accepts at all.

## Run

```bash
ctest -R test_c_tcp_s_stats --output-on-failure --test-dir build
```
