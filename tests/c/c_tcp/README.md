# c_tcp test

Unit tests for the `C_TCP` client GClass. Exercises connect/disconnect cycles, I/O with a local echo server (`pepon`), and timeout handling.

`test8` (`main_test8.c` + `c_test8.c`) is a `C_IOGATE` with `send_type` 1 (send to all), a `C_TCP_S` and 3 channels (`C_PROT_TCP4H` over `C_TCP`), and 3 raw peers. One `EV_SEND_MESSAGE` with a gbuffer to the gate: each peer must read the whole TCP4H frame (its 4-byte length, then the message). Up to 7.25.20 the channels shared the gbuffer, the first read it out, and peers 2 and 3 got an empty frame (red: *"a peer did not get the whole message"*, peers 1 and 2).

`test7` (`main_test7.c` + `c_test7.c`) drops a connected `C_TCP` and, while the drop waits in `ST_WAIT_STOPPED` for its read to be cancelled, sends it data (two messages, 13 bytes) -- what the layers above do until `EV_DISCONNECTED` tells them. The data must go away with no error, as the pending queue of a dead connection does, with ONE warning for the connection (*"tcp data sent while the connection closes, dropped"*, `dropped_msgs` 2, `dropped_bytes` 13), and the stop must end. Then a second `C_TCP` is dropped, sent `gone`, stopped and DESTROYED while it still waits in `ST_WAIT_STOPPED`: its drop must be said too, at the destroy (*"tcp data sent while the connection closes, dropped"*, `dropped_msgs` 1, `dropped_bytes` 4). Last, a third `C_TCP` with `timeout_inactivity` 300 ms connects and stays idle: its `disconnect_cause` must be `Inactivity timeout`, and the first one's `Local dropping`, read at their `EV_DISCONNECTED` and after the close (up to 7.25.20 the cancel of their read, `Operation canceled`, replaced them). Up to 7.25.10 `ST_WAIT_STOPPED` did not declare `EV_TX_DATA`: *"Event NOT DEFINED in state"* per message (red); up to 7.25.20 the data went with no trace at the default levels (red: *"Expected error not consumed"*).

`test6` (`main_test6.c` + `c_test6.c`) connects a `C_TCP` to a port where nobody listens: it waits in `ST_DISCONNECTED` for its reconnect timer, and is stopped there, started again and stopped again. Each stop must publish `EV_STOPPED` once and end in `ST_STOPPED`, and the start after it must work. Up to 7.25.4 this stop published nothing and kept the connect event: the next start failed with *"yev_connect ALREADY exists"* and the client never connected again (red).

`test5` (`main_test5.c` + `c_test5.c`) gives a connected `C_TCP` a write that does not start (an EMPTY gbuffer: `yev_start_event()` answers -1, as it does without memory to keep a submission). The connection must be dropped (*"Cannot start a write: the connection is dropped"*, `EV_DISCONNECTED`), the write event and its gbuffer freed, and a stop must reach `ST_STOPPED`. Up to 7.25.4 the -1 was ignored: the connection stayed up, the event and its gbuffer leaked, and the stop waited in `ST_WAIT_STOPPED` for ever (red: *"a write that did not start did not drop the connection"*, *"the stop ... did not end"*, *"system memory not free"*).

## Run

```bash
ctest -R '^c_tcp/' --output-on-failure --test-dir build
```
