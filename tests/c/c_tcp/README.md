# c_tcp test

Unit tests for the `C_TCP` client GClass. Exercises connect/disconnect cycles, I/O with a local echo server (`pepon`), and timeout handling.

`test5` (`main_test5.c` + `c_test5.c`) gives a connected `C_TCP` a write that does not start (an EMPTY gbuffer: `yev_start_event()` answers -1, as it does without memory to keep a submission). The connection must be dropped (*"Cannot start a write: the connection is dropped"*, `EV_DISCONNECTED`), the write event and its gbuffer freed, and a stop must reach `ST_STOPPED`. Up to 7.25.4 the -1 was ignored: the connection stayed up, the event and its gbuffer leaked, and the stop waited in `ST_WAIT_STOPPED` for ever (red: *"a write that did not start did not drop the connection"*, *"the stop ... did not end"*, *"system memory not free"*).

## Run

```bash
ctest -R '^c_tcp/' --output-on-failure --test-dir build
```
