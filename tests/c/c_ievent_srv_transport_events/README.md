# c_ievent_srv_transport_events test

Tests that a peer of `C_IEVENT_SRV` (`kernel/c/root-linux/src/c_ievent_srv.c`)
cannot send the events of the transport itself, nor the events a treedb
publishes.

One yuno holds both sides: a gate `__input_side__` (`C_IOGATE` -> `C_TCP_S`,
channels of `C_CHANNEL` -> `C_IEVENT_SRV` -> `C_WEBSOCKET` -> `C_TCP`) on
`ws://127.0.0.1:7898`, subscribed by the test's service `tester`, and a raw
websocket client (`C_IOGATE` -> `C_CHANNEL` -> `C_WEBSOCKET` -> `C_TCP`, no
`C_IEVENT_CLI`) whose frames the test writes by hand.

The client sends a good identity card (the session opens: the gate's one
`EV_ON_OPEN`), then, in the session, addressed to `tester`:

1. the six events a transport publishes on its own: `EV_ON_OPEN` with a
   `forged` key, an identity card and a peer certificate of its own,
   `EV_ON_CLOSE`, `EV_ON_MESSAGE`, `EV_ON_IEV_MESSAGE`, `EV_ON_ID`,
   `EV_ON_ID_NAK` (the last three declared public by `tester`, as the
   decoders of a gate do with `EV_ON_ID`, so the frame reads);
2. the five events a treedb publishes: `EV_TREEDB_NODE_CREATED`, `_UPDATED`,
   `_DELETED`, `_LINKED`, `_UNLINKED`, each with a `users` node `admin`
   (`tester` has them as actions, as a yuno subscribed to its own treedb
   does);
3. 50 frames of `EV_TRANGER_RECORD_ADDED`, public in `C_TRANGER` and not an
   event of `tester`;
4. `EV_TEST_LEGIT`, an event `tester` has as public.

The six are refused with ONE warning (*"event of the transport itself sent by
a peer, refused"*, `MSGSET_PROTOCOL`), the five with another (*"event of a
treedb sent by a peer, refused"*, `MSGSET_AUTH`), each with the `peername`, at
most once per 10 s per channel, and the gate publishes none of them. The 50
frames `tester` does not take are refused by the gate (`C_IOGATE`) with ONE
warning (*"event of a peer the yuno does not take now, refused"*,
`MSGSET_PROTOCOL`, the subscriber, its state, the channel, the `peername`),
and the 49 not said are said once when the gate stops (*"Events of a peer
refused, not said"*, `"not_said": 49`); no *"Event NOT DEFINED in state"*.
The last is published and ends the test, which checks then that the gate
published one open (the session's) and none of the eleven.

Up to 7.30.26 the gate published all of them as its own, with the kw the peer
wrote, and each of the 50 was an *"Event NOT DEFINED in state"* in `tester`,
an ERROR with a stack.

Mutants killed: either check removed; any one name taken out of either list
(`EV_ON_CLOSE`, `EV_ON_IEV_MESSAGE`, `EV_ON_ID`, `EV_ON_ID_NAK`,
`EV_TREEDB_NODE_UPDATED`, `_DELETED`, `_LINKED` tried); the per-channel brake
of the warning removed; the gate's check of what its subscriber takes
removed (50 ERRORs); its per-channel brake removed (50 warnings); the end
of the refusals at the gate's stop removed.
