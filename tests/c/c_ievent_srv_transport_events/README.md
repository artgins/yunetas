# c_ievent_srv_transport_events test

Tests that a peer of `C_IEVENT_SRV` (`kernel/c/root-linux/src/c_ievent_srv.c`)
cannot send the events of the transport itself.

One yuno holds both sides: a gate `__input_side__` (`C_IOGATE` -> `C_TCP_S`,
channels of `C_CHANNEL` -> `C_IEVENT_SRV` -> `C_WEBSOCKET` -> `C_TCP`) on
`ws://127.0.0.1:7898`, subscribed by the test's service `tester`, and a raw
websocket client (`C_IOGATE` -> `C_CHANNEL` -> `C_WEBSOCKET` -> `C_TCP`, no
`C_IEVENT_CLI`) whose frames the test writes by hand.

The client sends a good identity card (the session opens: the gate's one
`EV_ON_OPEN`), then, in the session, addressed to `tester`:

1. `EV_ON_OPEN` with a `forged` key, an identity card and a peer certificate
   of its own;
2. `EV_ON_CLOSE`;
3. `EV_ON_MESSAGE`;
4. `EV_ON_ID` (which `tester` declares public, as the decoders of a gate do);
5. `EV_TEST_LEGIT`, an event `tester` has as public.

The first four are refused with ONE warning (*"event of the transport itself
sent by a peer, refused"*, `MSGSET_PROTOCOL`, the `peername`, at most once per
10 s per channel) and the gate publishes none of them; the fifth is published
and ends the test, which checks then that the gate published one open (the
session's), no close, no message and no identification.

Up to 7.30.26 the gate published the four as its own, with the kw the peer
wrote.

Mutants killed: the check removed; `EV_ON_ID` taken out of the list; the
per-channel brake of the warning removed (four warnings).
