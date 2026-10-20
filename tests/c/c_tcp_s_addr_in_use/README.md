# c_tcp_s_addr_in_use test

Tests a `C_TCP_S` whose address is in use at its start. A yuno killed and
launched at once (the agent's restart of a node, `deactivate-snap`) finds its
port held by the dead one for a while: the io_uring ring of a process is torn
down after the process is gone, with the accept that holds the listening
socket. Up to 7.27.3 the start failed at once, the yuno exited
(`exitOnError`), and its watcher launched it again 2 s later with a *"Daemon
relaunched"* ERROR. The server now listens again, at each tick of the yuno's
periodic timeout, for `listen_retry_msec`.

The port `127.0.0.1:7796` is held by a raw listening socket. The yuno's tick
is 50 ms, and the test's `C_TIMER` subscribes to it before the server's, so
on a tick where both expire the test's fires first: the count of refused
binds is exact.

- **held at the start, released before the retry**: one `bind() FAILED`
  (a WARNING, from `yev_create_accept_event()`), *"Address in use: listening
  again"*, and at the retry *"Listening: its address in use was released"*;
  the server is in `ST_IDLE` and a client is accepted;
- **held longer than `listen_retry_msec`** (1 ms): the retry is refused too,
  and the start fails as any other listen (*"yev_create_accept_event()
  FAILED"*, an ERROR with `errno` 98);
- **`listen_retry_msec` 0**: no retry, the start fails at once.

## Run

```bash
ctest -R test_c_tcp_s_addr_in_use --output-on-failure --test-dir build
```
