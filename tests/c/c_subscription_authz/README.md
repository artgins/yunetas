# c_subscription_authz test

Tests the authorization of an external subscription (`EVF_AUTHZ_SUBSCRIBE`),
checked by `C_IEVENT_SRV` when the yuno sets `enable_subscription_authz`,
and two things of the same stack that a subscription goes through.

One yuno holds both sides: a `C_IEVENT_SRV` gate on `ws://127.0.0.1:7794`, a
`publisher` service whose `read` permission is aliased `__subscribe_event__`
(as `C_NODE`'s is), a real `C_NODE` service `treedb_subs_authz`, a real
`C_TRANGER` service `tranger_subs_authz`, and four
`C_IEVENT_CLI` authenticated as `nobody`, `nobody`, `reader` and `reader` by the test's
authentication parser. It checks:

- the channel commands of `C_IOGATE` (`view-channels`, `enable-channel`,
  `disable-channel`, `trace-on-channel`, `trace-off-channel`,
  `reset-stats-channel`) with a `channel_name` that matches no channel
  answer with no channel (up to 7.25.4 each looped for ever and blocked the
  event loop), and a matching one still selects its channels;
- gate off: `nobody` subscribes a flagged event, of `publisher` and
  `EV_TREEDB_NODE_UPDATED` of the `C_NODE`, as up to 7.25.4;
- gate on: `nobody` is refused all three -- the tranger's realtime feed
  `EV_TRANGER_RECORD_ADDED` too, open to any user up to 7.25.4 -- (logged,
  the channel stays open), an
  event without the flag is still subscribed, `reader` is accepted, and the
  checker is asked `read`; the global `authzs` trace is on meanwhile, and
  each of its lines carries the kw it checked (up to 7.25.4 it printed that
  kw after the checker had freed it; `main()` makes glibc fill every freed
  block, so the line came out without it);
- the feeds reach only the accepted subscriptions (a node update of the
  treedb included);
- withdrawing the refused subscriptions is not an error;
- `reader` is stopped with its remote subscriptions open and withdraws them
  before the close of its transport arrives: nothing is sent to the stopping
  transport (up to 7.25.4: *"Event NOT DEFINED in state"*), and the server
  drops them when the channel closes;
- a peer (`cli_hard`) asks `__hard_subscription__`, `__own_event__` and
  `__rename_event_name__` in the `__config__` of a subscription: only
  `__first_shot__` is kept (the rest is logged and dropped), so when the peer
  leaves its subscription goes with it, a later local subscriber still gets
  the feed, and the next user of the channel gets nothing it did not ask for
  (up to 7.25.4 the subscription was hard: it outlived the session, broke
  every publish before a later subscriber, and went to the next user);
- every channel is disabled and enabled again (`disable-channel`,
  `enable-channel`): the channel, its `C_IEVENT_SRV` and its `C_WEBSOCKET`
  run again and a client opens a session (the stop stops the protocol gobj,
  so the enable must start it again, or every client is accepted and never
  read);
- the gate is an autostart service, stopped by the yuno with a plain
  `gobj_stop()`: every protocol gobj of its channels stops with it (up to
  7.25.4: *"Destroying a RUNNING gobj"* for each `C_WEBSOCKET`).

## Run

```bash
ctest -R test_c_subscription_authz --output-on-failure --test-dir build
```
