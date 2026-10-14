# c_ievent_cli_peer_faults test

Tests what `C_IEVENT_CLI` (`kernel/c/root-linux/src/c_ievent_cli.c`) does with
the faults of the server it is connected to.

One yuno holds both sides: a real `C_IEVENT_CLI` (over `C_IOGATE` ->
`C_CHANNEL` -> `C_WEBSOCKET` -> `C_TCP`) and a raw websocket server on
`ws://127.0.0.1:7887` (`C_IOGATE` -> `C_TCP_S`, channels of `C_WEBSOCKET` ->
`C_TCP`, no `C_IEVENT_SRV`) whose frames the test writes by hand, as a server
that is not a yuno, or a broken one, would. Each identity card of the client
is answered with the next fault; the client closes the channel, connects
again and sends its card again:

0. a session event (`EV_MT_STATS`) before the session;
1. `EV_IDENTITY_CARD_ACK`, then a frame for another `dst_role`;
2. the ACK, then a frame for another `dst_yuno`;
3. the ACK, then a subscription (`__subscribing__`);
4. the ACK, then an unsubscription (`__unsubscribing__`);
5. the ACK alone: the session opens, and the test ends.

The names the server writes (`dst_role`, `dst_yuno`, `dst_service`) carry a
credential (`password=...`, `token=...`). Each fault is the server's, so it is
ONE warning (`MSGSET_PROTOCOL`, no stack), its names masked and capped, and no
credential reaches the log (`main.c` counts the lines of each, their priority,
and looks for the credentials in every line). A fault before the session also
gives the client's own warning, *"EV_ON_OPEN_ERROR: cannot open connection"*.

Up to 7.26.9 each fault was an error with the names in clear; both
subscriptions said *"subscription event ignored"*; and the session event
before the session went to `mt_inject_event()` (*"Not in session"*, an error)
before its own error.

## Run

```bash
ctest -R test_c_ievent_cli_peer_faults --output-on-failure --test-dir build
```
