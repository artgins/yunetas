# c_ievent_srv_identity_card test

Tests the identity cards a peer sends to `C_IEVENT_SRV`
(`kernel/c/root-linux/src/c_ievent_srv.c`), before its session.

One yuno holds both sides: a `C_IEVENT_SRV` gate on `ws://127.0.0.1:7813`
and a raw websocket client (`C_IOGATE` -> `C_CHANNEL` -> `C_WEBSOCKET` ->
`C_TCP`, no `C_IEVENT_CLI`) whose frames the test writes by hand, as a peer
that is not a yuno, or a misconfigured one, would. Each card but the last is
refused and its channel closed; the client connects again and sends the next:

0. a card without `__md_iev__`;
1. `dst_role` of another yuno, with a 4 KB key of its own;
2. `dst_yuno` of another yuno;
3. no `src_role`;
4. no `src_service`;
5. a `dst_service` that is not in this yuno;
6. a `jwt` that is not a string;
7. a command (`EV_MT_COMMAND`) before any card, carrying credentials: a
   `password` key, a `passw` and a `client_secret` down its kw, and
   `token=...` in its command line;
8. a good card: `EV_IDENTITY_CARD_ACK` with result 0, and a session.

Each refusal is caused by the peer, so it is ONE warning (`MSGSET_PROTOCOL`,
`peername`, the kw capped, no stack), and the jwt of the card is never
written to the log (`main.c` counts the lines of each, their priority and
their size, and looks for the jwt, and for the credentials of the command,
in every line). Only the good card opens a session.

Up to 7.25.20 each refusal was an error with the whole kw dumped (the jwt
with it), followed by a second error, *"event UNKNOWN in not-session
state"*, with the whole kw again; a card without `__md_iev__` added a third
error, with a stack; a command before the card was an error too; and a
`jwt` that was not a string was logged as an error by the reader and went on
to the authentication, which accepted the card.

## Run

```bash
ctest -R test_c_ievent_srv_identity_card --output-on-failure --test-dir build
```
