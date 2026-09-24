# c_tcp_s_ip_lists test

Tests the yuno's ip lists (`denied_ips`, `allowed_ips`) at `C_TCP_S` accept.
Raw client sockets bound to `127.0.1.x` connect to a `C_TCP_S` on
`127.0.0.1:7793`, so a list can name them while `127.0.0.1` stays the
exempt loopback peer:

- a peer in `denied_ips` is refused with or without `only_allowed_ips`, and
  it wins over `allowed_ips`;
- with `only_allowed_ips`, a peer not in `allowed_ips` is refused;
- loopback is accepted even when listed;
- the list key of a peername is its ip without the port, for ipv4, ipv6 and
  an ipv4 seen by a dual-stack socket.

## Run

```bash
ctest -R test_c_tcp_s_ip_lists --output-on-failure --test-dir build
```
