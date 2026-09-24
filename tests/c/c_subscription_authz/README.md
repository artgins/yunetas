# c_subscription_authz test

Tests the authorization of an external subscription (`EVF_AUTHZ_SUBSCRIBE`),
checked by `C_IEVENT_SRV` when the yuno sets `enable_subscription_authz`.

One yuno holds both sides: a `C_IEVENT_SRV` gate on `ws://127.0.0.1:7794`, a
`publisher` service whose `read` permission is aliased `__subscribe_event__`
(as `C_NODE`'s is), and three `C_IEVENT_CLI` authenticated as `nobody`,
`nobody` and `reader` by the test's authentication parser. It checks:

- gate off: `nobody` subscribes a flagged event, as up to 7.25.4;
- gate on: `nobody` is refused (logged, the channel stays open), an event
  without the flag is still subscribed, `reader` is accepted, and the checker
  is asked `read`;
- the feed reaches only the accepted subscriptions;
- withdrawing the refused subscription is not an error.

## Run

```bash
ctest -R test_c_subscription_authz --output-on-failure --test-dir build
```
