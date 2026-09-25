# c_subscriptions test

Tests subscribe / unsubscribe / publish semantics of the GObj framework. Verifies that events reach every subscriber and that unsubscribes (explicit and automatic on `gobj_destroy`) clean up properly.

- `test1`, `test2`: subscribe, publish and unsubscribe on a timer, with and
  without a `__filter__`; a repeated subscription is overridden.
- `test3`: the HARD subscriptions (`__hard_subscription__` in `__config__`).
  A repeated hard subscription is one: the second `gobj_subscribe_event()`
  logs a warning and returns the one there, and so does a plain subscription
  that matches it; each event arrives once (up to 7.25.4 a second one was
  made with no log, and each event arrived twice).
  `gobj_unsubscribe_event()` leaves a hard subscription and logs a warning;
  `gobj_unsubscribe_list()` with `force` removes it. A plain subscription
  repeated is still overridden, and a hard one over a plain one replaces it.
  A repeated `__own_event__` subscription, and a repeated
  `__rename_event_name__` one with a `__global__`, are overridden too (one
  subscription, each event once), and `gobj_unsubscribe_event()` with the
  same kw removes them (up to 7.25.4 each repeat was a second subscription,
  and the withdrawal found nothing).

## Run

```bash
ctest -R '^c_subscriptions/' --output-on-failure --test-dir build
```
