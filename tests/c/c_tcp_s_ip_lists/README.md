# c_tcp_s_ip_lists test

Tests the yuno's ip lists (`denied_ips`, `allowed_ips`) at `C_TCP_S` accept.
Raw client sockets bound to `127.0.1.x` connect to a `C_TCP_S` on
`127.0.0.1:7793`, so a list can name them while `127.0.0.1` stays the
exempt loopback peer:

- a peer in `denied_ips` is refused with or without `only_allowed_ips`, and
  it wins over `allowed_ips`;
- with `only_allowed_ips`, a peer not in `allowed_ips` is refused;
- a refusal is logged on the transition, one line a minute at most for each
  cause, and counted in the stat `refusedConnxs`: a denied peer that
  connects 5 times more writes no line (up to 7.25.4 each refusal wrote
  one);
- loopback is accepted even when listed;
- the list key of a peername is its ip without the port, for ipv4, ipv6 and
  an ipv4 seen by a dual-stack socket;
- the entries are kept in that form: `add-denied-ip` / `add-allowed-ip`
  store `2001:DB8::1` as `2001:db8::1` and `::ffff:203.0.113.7` as
  `203.0.113.7`, and refuse what is not a numeric ip (`[2001:db8::3]`,
  `203.0.113.8:443`, `localhost`); a link-local address is named with its
  interface (`fe80::2%lo` is stored `fe80::2%1`), which `allowed_ips`
  requires and `denied_ips` does not (without it the address is denied on
  every interface); the `remove-` commands take any form of the same ip,
  and answer an error for one not in the list;
- entries that 7.25.4 stored as typed (they come in the config here) are
  renamed at load to that form, or dropped with a warning when they are no
  ip; two entries of one ip are merged (a deny wins).

## Run

```bash
ctest -R test_c_tcp_s_ip_lists --output-on-failure --test-dir build
```
