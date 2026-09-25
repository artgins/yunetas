# c_tcp test

Unit tests for the `C_TCP` client GClass. Exercises connect/disconnect cycles, I/O with a local echo server (`pepon`), and timeout handling.

`test6` (`main_test6.c` + `c_test6.c`) connects a `C_TCP` to a port where nobody listens: it waits in `ST_DISCONNECTED` for its reconnect timer, and is stopped there, started again and stopped again. Each stop must publish `EV_STOPPED` once and end in `ST_STOPPED`, and the start after it must work. Up to 7.25.4 this stop published nothing and kept the connect event: the next start failed with *"yev_connect ALREADY exists"* and the client never connected again (red).

`test5` (`main_test5.c` + `c_test5.c`) gives a connected `C_TCP` a write that does not start (an EMPTY gbuffer: `yev_start_event()` answers -1, as it does without memory to keep a submission). The connection must be dropped (*"Cannot start a write: the connection is dropped"*, `EV_DISCONNECTED`), the write event and its gbuffer freed, and a stop must reach `ST_STOPPED`. Up to 7.25.4 the -1 was ignored: the connection stayed up, the event and its gbuffer leaked, and the stop waited in `ST_WAIT_STOPPED` for ever (red: *"a write that did not start did not drop the connection"*, *"the stop ... did not end"*, *"system memory not free"*).

## Run

```bash
ctest -R '^c_tcp/' --output-on-failure --test-dir build
```
