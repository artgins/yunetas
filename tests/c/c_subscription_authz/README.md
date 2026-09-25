# c_subscription_authz test

Tests the authorization of an external subscription (`EVF_AUTHZ_SUBSCRIBE`),
checked by `C_IEVENT_SRV` when the yuno sets `enable_subscription_authz`,
and two things of the same stack that a subscription goes through.

One yuno holds both sides: a `C_IEVENT_SRV` gate on `ws://127.0.0.1:7794`, a
`publisher` service whose `read` permission is aliased `__subscribe_event__`
(as `C_NODE`'s is), a real `C_NODE` service `treedb_subs_authz`, and three
`C_IEVENT_CLI` authenticated as `nobody`, `nobody` and `reader` by the test's
authentication parser. It checks:

- the channel commands of `C_IOGATE` (`view-channels`, `enable-channel`,
  `disable-channel`, `trace-on-channel`, `trace-off-channel`,
  `reset-stats-channel`) with a `channel_name` that matches no channel
  answer with no channel (up to 7.25.4 each looped for ever and blocked the
  event loop), and a matching one still selects its channels;
- gate off: `nobody` subscribes a flagged event, of `publisher` and
  `EV_TREEDB_NODE_UPDATED` of the `C_NODE`, as up to 7.25.4;
- gate on: `nobody` is refused both (logged, the channel stays open), an
  event without the flag is still subscribed, `reader` is accepted, and the
  checker is asked `read`;
- the feeds reach only the accepted subscriptions (a node update of the
  treedb included);
- withdrawing the refused subscriptions is not an error;
- `reader` is stopped with its remote subscriptions open and withdraws them
  before the close of its transport arrives: nothing is sent to the stopping
  transport (up to 7.25.4: *"Event NOT DEFINED in state"*), and the server
  drops them when the channel closes;
- the gate is an autostart service, stopped by the yuno with a plain
  `gobj_stop()`: every protocol gobj of its channels stops with it (up to
  7.25.4: *"Destroying a RUNNING gobj"* for each `C_WEBSOCKET`).

## Run

```bash
ctest -R test_c_subscription_authz --output-on-failure --test-dir build
```
