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
  and the listen fails: ONE ERROR, *"yev_create_accept_event() FAILED"*
  (`errno` 98), `listening` false, and the yuno goes on. Its first
  try (1 s later) is refused too and says nothing more (only the `bind()`
  WARNING); the port is released, and the second try (2 s after the first)
  listens, with one INFO, *"Listening again after a failed listen"*; a
  client is accepted;
- **`listen_retry_msec` 0**: no window, the listen fails at once (the same
  ERROR); its stop takes the try back, and with the port released nothing
  more is tried nor said;
- **a bad url**: an empty one at the start (*"URL NULL"*), then written while
  down: one of port 0 (*"Cannot Listen on port 0"*), one with no schema
  (*"Parsing url failed"*). Each is an ERROR, the yuno goes on, and nothing is
  tried by itself; writing the good url listens at once, and a client is
  accepted.

The server sets `"exitOnError": 0`, as the agent's do: without it the second
case exits the yuno (`exitOnError`, `-1` by default), as up to 7.30.17 and
again since 7.30.19: red, the test ends at *"Exiting"*.

`test_exit_on_error` checks those defaults. Each case is a yuno of its own in
a child process, whose exit code and output the parent reads:

| Case | Must |
|---|---|
| `C_TCP_S`, its port (7784) held, `listen_retry_msec` 0 | *"Exiting"*, code 255 (`-1`) |
| `C_TCP_S`, a TLS url (7792) whose certificate is not there | *"TLS server without its TLS context: exiting"*, code 0 |
| `C_TCP_S`, a url of port 0, `exitOnError` 7 | *"Cannot Listen on port 0"*, code 7 |
| `C_UDP_S`, its port (24301) held | *"exiting with code 0 (exitOnError)"*, code 0 |
| `C_QIOGATE`, its queue under a regular file | *"Queue cannot be opened: exiting"*, code 255 |

7.30.18 degraded every yuno and ignored `exitOnError`: red, each yuno reached
its end (code 3, *"it did not exit"*).

## Run

```bash
ctest -R "test_c_tcp_s_addr_in_use|test_exit_on_error" --output-on-failure --test-dir build
```
